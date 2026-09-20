#pragma once

/*
 * Common data carriers and tag types for the CPO/protocol level.
 *
 * Scatter descriptors, progress results, reserve/scatter cursors, alias tags,
 * `io_type`/`io_reserve_type` dispatch tags, seek positions, conversion
 * results, and related structural values are declared here. They give unrelated
 * device and object CPOs a stable common language. These carriers contain no
 * orchestration policy and invoking them performs no IO.
 */

namespace fast_io
{

template <typename T>
struct basic_io_scatter_t
{
	using value_type = T;
	T const *base;
	::std::size_t len;
};

/// @brief A character scatter carrying an explicit ordinary-cacheable-read assertion.
/// @details This wrapper deliberately has the same pointer/length field layout as `basic_io_scatter_t`, so entry decay
///          can preserve the provenance proof without enlarging the ABI transport object or retaining the identity of
///          an otherwise copy-stable producer. Layout equality only proves equal calling cost; consumers project a
///          value to the raw descriptor before use and must not type-pun between these distinct C++ types. Constructing
///          the wrapper is a caller assertion: `base[0, len)` must denote ordinary cacheable memory and remain readable
///          for the complete operation which may issue a prefetch. The wrapper does not independently prove lifetime,
///          bounds, non-nullness, or print borrowing.
///
///          A raw scatter intentionally has no implicit conversion to this type. Pointer shape alone cannot distinguish
///          normal allocations from MMIO, transient mappings, or implementation-defined non-cacheable storage. The
///          explicit constructor keeps every proof introduction visible at the audited source boundary.
template <typename T>
struct basic_prfch_cacheable_io_scatter_t
{
	using value_type = T;
	T const *base;
	::std::size_t len;

	inline constexpr basic_prfch_cacheable_io_scatter_t() noexcept = default;

	inline explicit constexpr basic_prfch_cacheable_io_scatter_t(
		T const *scatter_base, ::std::size_t scatter_len) noexcept
		: base(scatter_base), len(scatter_len)
	{}

	inline explicit constexpr basic_prfch_cacheable_io_scatter_t(basic_io_scatter_t<T> scatter) noexcept
		: base(scatter.base), len(scatter.len)
	{}

	inline constexpr basic_io_scatter_t<T> scatter() const noexcept
	{
		return {base, len};
	}
};

static_assert(sizeof(basic_prfch_cacheable_io_scatter_t<void>) == sizeof(basic_io_scatter_t<void>));
static_assert(alignof(basic_prfch_cacheable_io_scatter_t<void>) == alignof(basic_io_scatter_t<void>));
static_assert(offsetof(basic_prfch_cacheable_io_scatter_t<void>, base) ==
			  offsetof(basic_io_scatter_t<void>, base));
static_assert(offsetof(basic_prfch_cacheable_io_scatter_t<void>, len) ==
			  offsetof(basic_io_scatter_t<void>, len));

// should be binary compatible with POSIX's iovec

using io_scatter_t = basic_io_scatter_t<void>;
using io_scatters_t = basic_io_scatter_t<io_scatter_t>;

struct io_scatter_status_t
{
	::std::size_t position;
	::std::size_t position_in_scatter;
};

template <typename T>
struct basic_message_hdr
{
	T const *name;                    /* Optional address */
	::std::size_t namelen;            /* Size of address */
	basic_io_scatter_t<T> const *iov; /* Scatter/gather array */
	::std::size_t iovlen;             /* # elements in msg_iov */
	T const *control;                 /* Ancillary data, see below */
	::std::size_t controllen;         /* Ancillary data buffer len */
	int flags;                        /* Flags (unused) */

	inline operator basic_message_hdr<void>() const noexcept
		requires(!::std::same_as<T, void>)
	{
		/// @error: Should modify the internal size of basic_io_scatter_t instead of multiplying by the size of T
		return {name, namelen * sizeof(T), iov, iovlen, control, controllen, flags};
	}
};

using message_hdr = basic_message_hdr<void>;
// should be binary compatible with POSIX's msghdr

template <typename T>
struct io_type_t
{
	using type = T;
};
template <typename T>
inline constexpr io_type_t<T> io_type{};

template <::std::integral char_type>
struct cross_code_cvt_t
{
	using value_type = char_type;
	basic_io_scatter_t<value_type> scatter;
};

template <::std::integral char_type, typename T>
struct io_reserve_type_t
{
	inline explicit constexpr io_reserve_type_t() noexcept = default;
};
template <::std::integral char_type, typename T>
inline constexpr io_reserve_type_t<char_type, T> io_reserve_type{};

struct reserve_scatters_size_result
{
	::std::size_t scatters_size;
	::std::size_t reserve_size;
};

template <::std::integral char_type>
struct basic_reserve_scatters_define_result
{
	basic_io_scatter_t<char_type> *scatters_pos_ptr;
	char_type *reserve_pos_ptr;
};

/// @brief Result cursor pair for a producer that writes native byte-scatter descriptors.
/// @details This is intentionally a distinct type from `basic_reserve_scatters_define_result`: descriptor layout
///          compatibility does not permit a producer to write one class-template specialization through a pointer to
///          another. Byte lengths are expressed in bytes, while `reserve_pos_ptr` still advances in `char_type` units.
template <::std::integral char_type>
struct basic_reserve_scatters_bytes_define_result
{
	io_scatter_t *scatters_pos_ptr;
	char_type *reserve_pos_ptr;
};

struct io_alias_t
{
	inline explicit constexpr io_alias_t() noexcept = default;
};

inline constexpr io_alias_t io_alias{};

template <::std::integral char_type>
struct io_alias_type_t
{
	inline explicit constexpr io_alias_type_t() noexcept = default;
};

template <::std::integral char_type>
inline constexpr io_alias_type_t<char_type> io_alias_type{};

template <::std::integral char_type>
struct try_get_result
{
	char_type ch;
	bool eof;
};

template <typename in_char_type, typename out_char_type>
struct deco_result
{
	in_char_type const *input_result_ptr{};
	out_char_type *output_result_ptr{};
};

enum class seekdir : ::std::uint_least8_t
{
	beg = 0, // SEEK_SET
	cur = 1, // SEEK_CUR
	end = 2, // SEEK_END
};

using uintfpos_t = ::std::uintmax_t;
using intfpos_t = ::std::intmax_t;

struct io_construct_t
{
	inline explicit constexpr io_construct_t() noexcept = default;
};

inline constexpr io_construct_t io_construct{};

template <typename T>
struct io_cookie_type_t
{
	explicit inline constexpr io_cookie_type_t() noexcept = default;
};

template <typename T>
inline constexpr io_cookie_type_t<T> io_cookie_type{};

struct io_cookie_t
{
	explicit inline constexpr io_cookie_t() noexcept = default;
};

inline constexpr io_cookie_t io_cookie{};

struct io_null_t
{
	explicit inline constexpr io_null_t() noexcept = default;
};

inline constexpr io_null_t io_null{};

} // namespace fast_io
