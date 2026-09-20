#pragma once

namespace fast_io
{

namespace details
{

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *crypto_hash_pr_df_impl(::std::byte const *first, ::std::byte const *last,
												   char_type *iter) noexcept
{
	constexpr auto tb{::fast_io::details::digits_table<char_type, 16, uppercase>};
	constexpr ::std::size_t two{2};
	for (; first != last; ++first)
	{
		iter = non_overlapped_copy_n(tb + (static_cast<::std::uint_least8_t>(*first) << 1), two, iter);
	}
	return iter;
}

template <typename T>
inline void update_multiple_blocks(T *__restrict ctx, io_scatter_t const *base, ::std::size_t n) noexcept
{
	for (::std::size_t i{}; i != n; ++i)
	{
		io_scatter_t e{base[i]};
		ctx->update(reinterpret_cast<::std::byte const *>(e.base),
					reinterpret_cast<::std::byte const *>(e.base) + e.len);
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *pr_rsv_uuid(char_type *iter, ::std::byte const *uuid) noexcept
{
	auto next_it{uuid + 4}; // 4
	iter = crypto_hash_pr_df_impl<uppercase>(uuid, next_it, iter);
	for (::std::size_t i{}; i != 3; ++i) // 2*3=6
	{
		*iter = char_literal_v<u8'-', char_type>;
		++iter;
		uuid = next_it;
		next_it += 2;
		iter = crypto_hash_pr_df_impl<uppercase>(uuid, next_it, iter);
	}
	*iter = char_literal_v<u8'-', char_type>; // 6
	++iter;
	return crypto_hash_pr_df_impl<uppercase>(next_it, next_it + 6, iter);
}

inline constexpr ::std::size_t hex_encoding_prv_size_cal(::std::byte const *first, ::std::byte const *last) noexcept
{
	::std::size_t n{static_cast<::std::size_t>(last - first)};
	constexpr ::std::size_t mxn{::std::numeric_limits<::std::size_t>::max() / 2};
	if (n > mxn)
	{
		::fast_io::fast_terminate();
	}
	return n << 1;
}

} // namespace details

template <bool uppercase>
struct basic_hex_encode
{
	using manip_tag = manip_tag_t;
	::std::byte const *reference{};
	::std::byte const *last{};
};

template <::std::integral char_type, bool uppercase>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, ::fast_io::basic_hex_encode<uppercase>>,
												  ::fast_io::basic_hex_encode<uppercase> e) noexcept
{
	return ::fast_io::details::hex_encoding_prv_size_cal(e.reference, e.last);
}

template <::std::integral char_type, bool uppercase>
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, ::fast_io::basic_hex_encode<uppercase>>,
												 char_type *iter, ::fast_io::basic_hex_encode<uppercase> e) noexcept
{
	return ::fast_io::details::crypto_hash_pr_df_impl<uppercase>(e.reference, e.last, iter);
}

template <::std::integral ch_type, typename T>
struct basic_crypto_hash_as_file
{
	using output_char_type = ch_type;
	using manip_tag = manip_tag_t;
	using native_handle_type = T *;
	T *ptr{};
};

template <::std::integral ch_type, typename T>
inline constexpr basic_crypto_hash_as_file<ch_type, T>
output_stream_ref_define(basic_crypto_hash_as_file<ch_type, T> t) noexcept
{
	return t;
}

template <::std::integral char_type, typename T>
inline constexpr void require_secure_clear(basic_crypto_hash_as_file<char_type, T>) noexcept
{
}

template <::std::integral ch_type, typename T>
inline constexpr void write_all_bytes_overflow_define(basic_crypto_hash_as_file<ch_type, T> t, ::std::byte const *first,
													  ::std::byte const *last) noexcept
{
	t.ptr->update(first, last);
}

namespace manipulators
{

/// @brief Encodes the object representation of a contiguous iterator range as lowercase hexadecimal.
/// @details Elements must be trivially copyable; every byte in `[first, last)` is encoded in memory order as two hex
///          digits. The view is borrowed and the source must remain valid through formatting.
template <::std::contiguous_iterator Iter>
	requires ::std::is_trivially_copyable_v<::std::iter_value_t<Iter>>
inline constexpr ::fast_io::basic_hex_encode<false> hex_encode(Iter first, Iter last) noexcept
{
	if constexpr (::std::same_as<::std::iter_value_t<Iter>, ::std::byte>)
	{
		return {::std::to_address(first), ::std::to_address(last)};
	}
	else
	{
		return {reinterpret_cast<::std::byte const *>(::std::to_address(first)),
				reinterpret_cast<::std::byte const *>(::std::to_address(last))};
	}
}

/// @brief Encodes the object representation of a contiguous iterator range as uppercase hexadecimal.
/// @details Byte order and lifetime semantics match `hex_encode`; only `A-F` case changes.
template <::std::contiguous_iterator Iter>
	requires ::std::is_trivially_copyable_v<::std::iter_value_t<Iter>>
inline constexpr ::fast_io::basic_hex_encode<true> hex_encode_upper(Iter first, Iter last) noexcept
{
	if constexpr (::std::same_as<::std::iter_value_t<Iter>, ::std::byte>)
	{
		return {::std::to_address(first), ::std::to_address(last)};
	}
	else
	{
		return {reinterpret_cast<::std::byte const *>(::std::to_address(first)),
				reinterpret_cast<::std::byte const *>(::std::to_address(last))};
	}
}

/// @brief Encodes a contiguous trivially-copyable range as lowercase hexadecimal bytes.
/// @details The complete object representation, including padding bytes if present, is observed in memory order. The
///          range is borrowed and must outlive formatting.
template <::std::ranges::contiguous_range rg>
	requires ::std::is_trivially_copyable_v<::std::ranges::range_value_t<rg>>
inline constexpr ::fast_io::basic_hex_encode<false> hex_encode(rg &&r) noexcept
{
	return hex_encode(::std::to_address(::std::ranges::begin(r)), ::std::to_address(::std::ranges::end(r)));
}

/// @brief Encodes a contiguous trivially-copyable range as uppercase hexadecimal bytes.
/// @details Semantics match the lowercase range overload except for alphabetic digit case.
template <::std::ranges::contiguous_range rg>
	requires ::std::is_trivially_copyable_v<::std::ranges::range_value_t<rg>>
inline constexpr ::fast_io::basic_hex_encode<true> hex_encode_upper(rg &&r) noexcept
{
	return hex_encode_upper(::std::to_address(::std::ranges::begin(r)), ::std::to_address(::std::ranges::end(r)));
}

/// @brief Adapts a hash context to a byte sink with an explicitly selected character type.
/// @details Writes update `hashctx` rather than producing ordinary file output. The context is borrowed and must remain
///          alive; finalization is not performed automatically by the adapter.
template <::std::integral char_type, typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<char_type, T> basic_as_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

/// @brief Adapts a hash context to a `char` byte sink.
/// @details The returned observer forwards written bytes to `hashctx.update` and does not own or finalize the context.
template <typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<char, T> as_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

/// @brief Adapts a hash context to a `wchar_t` byte sink.
/// @details Character type selects the IO observer surface; hashing still consumes raw bytes.
template <typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<wchar_t, T> was_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

/// @brief Adapts a hash context to a `char8_t` byte sink.
/// @details The context is borrowed and receives raw write bytes without implicit finalization.
template <typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<char8_t, T> u8as_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

/// @brief Adapts a hash context to a `char16_t` byte sink.
/// @details The adapter changes only the observer's character type; hash updates remain byte-oriented.
template <typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<char16_t, T> u16as_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

/// @brief Adapts a hash context to a `char32_t` byte sink.
/// @details The returned observer borrows `hashctx` and forwards raw written bytes to it.
template <typename T>
inline constexpr ::fast_io::basic_crypto_hash_as_file<char32_t, T> u32as_file(T &hashctx) noexcept
{
	return {__builtin_addressof(hashctx)};
}

} // namespace manipulators

namespace manipulators
{

/// @brief Models a mutable hash context with incremental update and finalization operations.
/// @details The concept checks syntax only; algorithms using it may impose stronger state-machine requirements such as
///          forbidding updates after finalization.
template <typename T>
concept crypto_hash_context = requires(T t, ::std::byte *ptr) {
	t.update(ptr, ptr);
	t.do_final();
};

/// @brief Models a hash context whose digest size is a compile-time member.
/// @details `digest_size` is interpreted as a byte count by digest formatting.
template <typename T>
concept compile_time_size_crypto_hash_context = requires(T t) { ::std::remove_cvref_t<T>::digest_size; };

/// @brief Models a hash context that reports its digest size at run time.
/// @details `runtime_digest_size()` must yield a byte count convertible to `size_t`.
template <typename T>
concept runtime_size_crypto_hash_context = requires(T t) {
	{ t.runtime_digest_size() } -> ::std::convertible_to<::std::size_t>;
};

/// @brief Selects the external representation of a finalized hash digest.
/// @details `lower` and `upper` encode each digest byte as two hexadecimal digits; `raw_bytes` emits the digest bytes
///          directly and can therefore contain zero and non-text code units.
enum class digest_format
{
	lower,
	upper,
	raw_bytes
};

/// @brief Non-owning request to print the current/finalized digest of a hash context.
/// @details The context lifetime and required finalization state are the caller's responsibility. `d` determines text
///          hexadecimal versus raw-byte output.
template <digest_format d, typename T>
struct hash_digest_t
{
	using manip_tag = manip_tag_t;
	using reference_type = T;
	reference_type reference;
};

/// @brief Emits a hash context's digest as lowercase hexadecimal.
/// @details The context is borrowed; the manipulator does not imply ownership and callers must satisfy its digest-state
///          contract before formatting.
template <crypto_hash_context ctx>
inline constexpr hash_digest_t<digest_format::lower, ctx const &> hash_digest(ctx const &r) noexcept
{
	return {r};
}

/// @brief Emits a hash context's digest as uppercase hexadecimal.
/// @details Digest bytes and ordering match `hash_digest`; only hexadecimal case differs.
template <crypto_hash_context ctx>
inline constexpr hash_digest_t<digest_format::upper, ctx const &> hash_digest_upper(ctx const &r) noexcept
{
	return {r};
}

/// @brief Emits a hash context's digest as raw bytes.
/// @details Output is binary, not textual, and may contain null bytes. The context is borrowed.
template <crypto_hash_context ctx>
inline constexpr hash_digest_t<digest_format::raw_bytes, ctx const &> hash_digest_raw_bytes(ctx const &r) noexcept
{
	return {r};
}

/// @brief Hidden carrier for hashing/compressing a borrowed byte range and formatting the resulting digest.
/// @details `base`/`len` describe raw bytes; `d` selects lowercase, uppercase, or raw digest output. The producing
///          factory does not own the range.
template <digest_format d, crypto_hash_context T>
struct hash_compress_t
{
	using manip_tag = manip_tag_t;
	using reference_type = T;
	::std::byte const *base{};
	::std::size_t len{};
};

/// @brief Hashes a contiguous trivially-copyable range and emits the digest as lowercase hexadecimal.
/// @details The range's complete object representation is consumed in memory order; padding bytes are included. The
///          range remains borrowed through formatting.
template <crypto_hash_context ctx, ::std::ranges::contiguous_range T>
	requires(::std::is_trivially_copyable_v<::std::ranges::range_value_t<T>> && !::std::is_array_v<T>)
inline constexpr hash_compress_t<digest_format::lower, ctx> hash_compress(T const &t) noexcept
{
	if constexpr (::std::same_as<::std::ranges::range_value_t<T>, ::std::byte>)
	{
		return {::std::ranges::data(t), ::std::ranges::size(t)};
	}
	else
	{
		return {reinterpret_cast<::std::byte const *>(::std::ranges::data(t)),
				static_cast<::std::size_t>(::std::ranges::size(t)) * sizeof(::std::ranges::range_value_t<T>)};
	}
}

/// @brief Hashes a contiguous trivially-copyable range and emits the digest as uppercase hexadecimal.
/// @details Input bytes match `hash_compress`; only digest hex case differs.
template <crypto_hash_context ctx, ::std::ranges::contiguous_range T>
	requires(::std::is_trivially_copyable_v<::std::ranges::range_value_t<T>> && !::std::is_array_v<T>)
inline constexpr hash_compress_t<digest_format::upper, ctx> hash_compress_upper(T const &t) noexcept
{
	if constexpr (::std::same_as<::std::ranges::range_value_t<T>, ::std::byte>)
	{
		return {::std::ranges::data(t), ::std::ranges::size(t)};
	}
	else
	{
		return {reinterpret_cast<::std::byte const *>(::std::ranges::data(t)),
				static_cast<::std::size_t>(::std::ranges::size(t)) * sizeof(::std::ranges::range_value_t<T>)};
	}
}

/// @brief Hashes a contiguous trivially-copyable range and emits the digest bytes directly.
/// @details The output is binary and may contain arbitrary byte values; the source range is borrowed.
template <crypto_hash_context ctx, ::std::ranges::contiguous_range T>
	requires(::std::is_trivially_copyable_v<::std::ranges::range_value_t<T>> && !::std::is_array_v<T>)
inline constexpr hash_compress_t<digest_format::raw_bytes, ctx> hash_compress_raw_bytes(T const &t) noexcept
{
	if constexpr (::std::same_as<::std::ranges::range_value_t<T>, ::std::byte>)
	{
		return {::std::ranges::data(t), ::std::ranges::size(t)};
	}
	else
	{
		return {reinterpret_cast<::std::byte const *>(::std::ranges::data(t)),
				static_cast<::std::size_t>(::std::ranges::size(t)) * sizeof(::std::ranges::range_value_t<T>)};
	}
}

} // namespace manipulators

namespace details
{
template <typename T>
concept context_digest_to_byte_ptr_runtime_impl = requires(T t, ::std::byte *ptr) {
	{ t.digest_to_byte_ptr(ptr) } -> ::std::same_as<::std::byte *>;
};

template <typename T>
concept context_digest_byte_ptr_impl = requires(T t) {
	{ t.digest_byte_ptr() } -> ::std::same_as<::std::byte const *>;
};

template <::fast_io::manipulators::digest_format d, ::std::size_t digest_size>
	requires(static_cast<::std::size_t>(d) < static_cast<::std::size_t>(3))
inline constexpr ::std::size_t cal_crypto_hash_resrv_size() noexcept
{
	static_assert(digest_size <= SIZE_MAX / 2);
	constexpr ::std::size_t v{d == ::fast_io::manipulators::digest_format::raw_bytes ? digest_size
																					 : (digest_size << 1u)};
	return v;
}

template <::fast_io::manipulators::digest_format d, ::std::size_t digest_size>
inline constexpr ::std::size_t crypto_hash_resrv_size_cache{cal_crypto_hash_resrv_size<d, digest_size>()};

template <::fast_io::manipulators::digest_format d, ::std::integral char_type>
inline constexpr char_type *copy_to_hash_df_commom_impl(char_type *iter, ::std::byte const *buffer,
														::std::size_t digest_size) noexcept
{
	if constexpr (d == ::fast_io::manipulators::digest_format::raw_bytes)
	{
#if __cpp_lib_bit_cast >= 201806L
		if (__builtin_is_constant_evaluated())
		{
			for (::std::size_t i{}; i != digest_size; ++i)
			{
				*iter = ::std::to_integer<char unsigned>(buffer[i]);
				++iter;
			}
			return iter;
		}
		else
#endif
		{
			return ::fast_io::details::non_overlapped_copy_n(reinterpret_cast<char unsigned const *>(buffer),
															 digest_size, iter);
		}
	}
	else
	{
		return ::fast_io::details::crypto_hash_pr_df_impl < d == ::fast_io::manipulators::digest_format::upper > (buffer, buffer + digest_size, iter);
	}
}

template <::fast_io::manipulators::digest_format d, typename T, ::std::integral char_type>
inline constexpr char_type *prv_srv_hash_df_common_impl(char_type *iter, T const &t) noexcept
{
	if constexpr (::fast_io::manipulators::compile_time_size_crypto_hash_context<T>)
	{
		constexpr ::std::size_t digest_size{::std::remove_cvref_t<T>::digest_size};
		::std::byte buffer[digest_size];
		if constexpr (context_digest_to_byte_ptr_runtime_impl<T>)
		{
			::std::size_t diff{static_cast<::std::size_t>(t.digest_to_byte_ptr(buffer) - buffer)};
			return copy_to_hash_df_commom_impl<d>(iter, buffer, diff);
		}
		else
		{
			t.digest_to_byte_ptr(buffer);
			return copy_to_hash_df_commom_impl<d>(iter, buffer, digest_size);
		}
	}
	else
	{
		::std::size_t digest_size{t.runtime_digest_size()};
		if constexpr (::fast_io::details::context_digest_byte_ptr_impl<T>)
		{
			::std::byte const *ptr{t.digest_byte_ptr()};
			return copy_to_hash_df_commom_impl<d>(iter, ptr, digest_size);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<::std::byte> bufferf(digest_size);
			::std::byte *buffer{bufferf.ptr};
			if constexpr (context_digest_to_byte_ptr_runtime_impl<T>)
			{
				::std::size_t diff{static_cast<::std::size_t>(t.digest_to_byte_ptr(buffer) - buffer)};
				return copy_to_hash_df_commom_impl<d>(iter, buffer, diff);
			}
			else
			{
				t.digest_to_byte_ptr(buffer);
				return copy_to_hash_df_commom_impl<d>(iter, buffer, digest_size);
			}
		}
	}
}

// this function may possibly be useless because scan_freestanding will correctly deal with context_scannable
template <::fast_io::manipulators::digest_format d, typename T, ::std::random_access_iterator Iter>
inline constexpr Iter prv_srv_hash_df_impl(Iter iter, T const &t) noexcept
{
	if constexpr (::std::contiguous_iterator<Iter> && !::std::is_pointer_v<Iter>)
	{
		return ::fast_io::details::prv_srv_hash_df_impl<d>(::std::to_address(iter), t) - ::std::to_address(iter) + iter;
	}
	else
	{
		using char_type = ::std::iter_value_t<Iter>;
		if constexpr (d == ::fast_io::manipulators::digest_format::raw_bytes)
		{
			if (__builtin_is_constant_evaluated())
			{
				return ::fast_io::details::prv_srv_hash_df_common_impl<d>(iter, t);
			}
			else
			{
				if constexpr (sizeof(char_type) == 1 && ::std::is_pointer_v<Iter>)
				{
					if constexpr (context_digest_to_byte_ptr_runtime_impl<T>)
					{
						return t.digest_to_byte_ptr(reinterpret_cast<::std::byte *>(iter));
					}
					else
					{
						constexpr ::std::size_t digest_size{::std::remove_cvref_t<T>::digest_size};
						t.digest_to_byte_ptr(reinterpret_cast<::std::byte *>(iter));
						return iter + digest_size;
					}
				}
				else
				{
					return ::fast_io::details::prv_srv_hash_df_common_impl<d>(iter, t);
				}
			}
		}
		else
		{
			return ::fast_io::details::prv_srv_hash_df_common_impl<d>(iter, t);
		}
	}
}

template <typename T>
inline constexpr ::std::byte *cal_hash_internal_impl(::std::byte const *base, ::std::size_t len,
													 ::std::byte *buffer) noexcept
{
	T t;
	t.update(base, base + len);
	t.do_final();
	return t.digest_to_byte_ptr(buffer);
}

template <typename T>
inline constexpr void cal_hash_internal(::std::byte const *base, ::std::size_t len, ::std::byte *buffer) noexcept
{
	T t;
	t.update(base, base + len);
	t.do_final();
	t.digest_to_byte_ptr(buffer);
}

template <::fast_io::manipulators::digest_format d, typename T, ::std::integral char_type>
inline constexpr char_type *prv_srv_hash_compress_df_impl(char_type *iter, ::std::byte const *base,
														  ::std::size_t len) noexcept
{
	constexpr ::std::size_t digest_size{::std::remove_cvref_t<T>::digest_size};
	if constexpr (context_digest_to_byte_ptr_runtime_impl<T>)
	{
		if constexpr (d == ::fast_io::manipulators::digest_format::raw_bytes)
		{
			if (__builtin_is_constant_evaluated())
			{
				::std::byte buffer[digest_size];
				auto ret{cal_hash_internal_impl<T>(base, len, buffer)};
				return ::fast_io::details::copy_to_hash_df_commom_impl < d ==
					   ::fast_io::manipulators::digest_format::upper > (iter, buffer, static_cast<::std::size_t>(ret - buffer));
			}
			else
			{
				if constexpr (sizeof(char_type) == 1)
				{
					return cal_hash_internal_impl<T>(base, len, reinterpret_cast<::std::byte *>(iter));
				}
				else
				{
					::std::byte buffer[digest_size];
					auto p{cal_hash_internal_impl<T>(base, len, buffer)};
					return ::fast_io::details::copy_to_hash_df_commom_impl<d>(iter, buffer,
																			  static_cast<::std::size_t>(p - buffer));
				}
			}
		}
		else
		{
			::std::byte buffer[digest_size];
			auto p{cal_hash_internal_impl<T>(base, len, buffer)};
			return ::fast_io::details::copy_to_hash_df_commom_impl<d>(iter, buffer,
																	  static_cast<::std::size_t>(p - buffer));
		}
	}
	else
	{
		if constexpr (d == ::fast_io::manipulators::digest_format::raw_bytes)
		{
			if (__builtin_is_constant_evaluated())
			{
				::std::byte buffer[digest_size];
				cal_hash_internal<T>(base, len, buffer);
				return ::fast_io::details::copy_to_hash_df_commom_impl < d ==
					   ::fast_io::manipulators::digest_format::upper > (iter, buffer, digest_size);
			}
			else
			{
				if constexpr (sizeof(char_type) == 1)
				{
					cal_hash_internal<T>(base, len, reinterpret_cast<::std::byte *>(iter));
					return iter + digest_size;
				}
				else
				{
					::std::byte buffer[digest_size];
					cal_hash_internal<T>(base, len, buffer);
					return ::fast_io::details::copy_to_hash_df_commom_impl<d>(iter, buffer, digest_size);
				}
			}
		}
		else
		{
			::std::byte buffer[digest_size];
			cal_hash_internal<T>(base, len, buffer);
			return ::fast_io::details::copy_to_hash_df_commom_impl<d>(iter, buffer, digest_size);
		}
	}
}

} // namespace details

/// @feature concept:static_precise_size
template <::std::integral char_type, ::fast_io::manipulators::digest_format d,
		  ::fast_io::manipulators::compile_time_size_crypto_hash_context T>
	requires(static_cast<::std::size_t>(d) < static_cast<::std::size_t>(3))
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::hash_digest_t<d, T const &>>) noexcept
{
	return ::fast_io::details::crypto_hash_resrv_size_cache<d, ::std::remove_cvref_t<T>::digest_size>;
}

template <::std::integral char_type, ::fast_io::manipulators::digest_format d,
		  ::fast_io::manipulators::runtime_size_crypto_hash_context T>
	requires(static_cast<::std::size_t>(d) < static_cast<::std::size_t>(3))
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::hash_digest_t<d, T const &>>,
				   ::fast_io::manipulators::hash_digest_t<d, T const &> digest) noexcept
{
	if constexpr (d == ::fast_io::manipulators::digest_format::raw_bytes)
	{
		return digest.reference.runtime_digest_size();
	}
	else
	{
		::std::size_t dgst_size{digest.reference.runtime_digest_size()};
		constexpr ::std::size_t half_size{SIZE_MAX / 2u};
		if (dgst_size > half_size)
		{
			fast_terminate();
		}
		return dgst_size << 1u;
	}
}

template <::std::integral char_type, ::fast_io::manipulators::digest_format d,
		  ::fast_io::manipulators::runtime_size_crypto_hash_context T>
	requires(static_cast<::std::size_t>(d) < static_cast<::std::size_t>(3))
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::hash_digest_t<d, T const &>>) noexcept
{
	return ::fast_io::details::dynamic_reserve_default_static_stack_size<char_type>();
}

template <::fast_io::manipulators::digest_format d, ::fast_io::manipulators::crypto_hash_context T,
		  ::std::integral char_type>
inline constexpr char_type *
print_reserve_define(::fast_io::io_reserve_type_t<char_type, ::fast_io::manipulators::hash_digest_t<d, T const &>>,
					 char_type *iter, ::fast_io::manipulators::hash_digest_t<d, T const &> t) noexcept
{
	return ::fast_io::details::prv_srv_hash_df_impl<d>(iter, t.reference);
}

template <::std::integral char_type, ::fast_io::manipulators::digest_format d,
		  ::fast_io::manipulators::crypto_hash_context T>
	requires(static_cast<::std::size_t>(d) < static_cast<::std::size_t>(3))
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::hash_compress_t<d, T>>) noexcept
{
	return ::fast_io::details::crypto_hash_resrv_size_cache<d, ::std::remove_cvref_t<T>::digest_size>;
}
template <::fast_io::manipulators::digest_format d, ::fast_io::manipulators::crypto_hash_context T,
		  ::std::integral char_type>
inline constexpr char_type *
print_reserve_define(::fast_io::io_reserve_type_t<char_type, ::fast_io::manipulators::hash_compress_t<d, T>>,
					 char_type *iter, ::fast_io::manipulators::hash_compress_t<d, T> t) noexcept
{
	return ::fast_io::details::prv_srv_hash_compress_df_impl<d, T>(iter, t.base, t.len);
}
} // namespace fast_io
