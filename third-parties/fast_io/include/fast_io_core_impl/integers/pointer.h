#pragma once

#include <utility>

namespace fast_io
{

/*
we do not accept char const* since we never know whether it has null terminator.
This avoids security vulneralbilities for
		char * str = 0;
		print(str);
Instead, we print out its pointer value

We extend print pointers to print contiguous_iterator. No we can write things like

::std::vector<::std::size_t> vec(100,2);
println("vec.begin():",fast_io::mnp::pointervw(vec.begin())," vec.end()",fast_io::mnp::pointervw(vec.end()));
*/
namespace manipulators
{
/// @brief Hidden carrier that forces one integral code unit to be rendered as a character.
/// @details This disambiguates character output from numeric integral formatting; `reference` is copied by value.
template <typename T>
struct chvw_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Non-owning character scatter whose run-time length is bounded by compile-time capacity `N`.
/// @details Every constructor and consumer enforces `len <= N`; exceeding the bound terminates because reserve output
///          relies on `N` as a hard write-capacity proof. The referenced characters remain caller-owned.
template <::std::integral ch_type, ::std::size_t N>
struct small_scatter_t
{
	using manip_tag = manip_tag_t;
	ch_type const *base{};
	::std::size_t len{};

	/// @brief Constructs an empty bounded scatter.
	/// @details `base` is null and `len` is zero; assigning fields later must still preserve `len <= N`.
	inline constexpr small_scatter_t() noexcept = default;

	/// @brief Constructs a run-time extent whose capacity remains bounded by the type-level reserve proof.
	/// @details `print_reserve_size` advertises exactly `N`, so permitting `len > N` would make the public aggregate
	///          reserve-printable while its define CPO writes past the selected destination. A user-provided constructor
	///          removes aggregate initialization as an invariant bypass. The fields remain readable for source
	///          compatibility, and every consuming CPO revalidates the bound in case mutable legacy code changes `len`.
	inline constexpr small_scatter_t(ch_type const *scatter_base, ::std::size_t scatter_len) noexcept
		: base(scatter_base), len(scatter_len)
	{
		validate();
	}

	/// @brief Enforces the type-level capacity invariant.
	/// @details Terminates when `len > N`; it performs no pointer-validity or lifetime check.
	inline constexpr void validate() const noexcept
	{
		if (N < len) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
	}
};

/// @brief A finite C-string view which stays scatter-based until print/concat proves it a short compiler constant.
/// @details `len` preserves C-string/precision semantics while `extent` proves the maximum readable source range.
///          Ordinary printing projects this object directly to a scatter.  Its separate compiler-constant CPO below
///          may rebind a fully known short value to `small_scatter_t` for contiguous field merging. Producers must
///          supply `len <= extent`, like the pointer/length invariant of `basic_io_scatter_t`; the format frontend's
///          bounded C-string search establishes that fact without adding a redundant run-time branch to printing.
template <::std::integral ch_type, ::std::size_t extent>
struct bounded_cstr_scatter_t
{
	using manip_tag = manip_tag_t;
	ch_type const *base{};
	::std::size_t len{};

	inline constexpr bounded_cstr_scatter_t(
		ch_type const *scatter_base, ::std::size_t scatter_len) noexcept
		: base(scatter_base), len(scatter_len)
	{}

	[[nodiscard]] inline constexpr operator
		::fast_io::basic_io_scatter_t<ch_type>() const noexcept
	{
		return {base, len};
	}
};

/// @brief Non-owning character scatter whose exact length `N` is encoded in its type.
/// @details `base` must address at least `N` readable code units for the complete print operation. Conversion erases
///          the static length into an ordinary scatter without copying characters.
template <::std::integral ch_type, ::std::size_t N>
struct static_scatter_t
{
	using manip_tag = manip_tag_t;
	using value_type = ch_type;
	ch_type const *base{};
	inline constexpr operator ::fast_io::basic_io_scatter_t<ch_type>() const noexcept
	{
		return {base, N};
	}
};

/// @brief Renders one integral code unit as a character rather than as its numeric value.
/// @details The code unit is copied into a semantic carrier; no encoding conversion or validation is performed.
template <::std::integral T>
inline constexpr chvw_t<T> chvw(T ch) noexcept
{
	// Explicit single-character view. This marker avoids the ambiguous path where
	// character code units are both ::std::integral values and printable text.
	return {ch};
}

/// @brief Resolves a non-null terminated OS-string wrapper to its complete borrowed scatter.
/// @details Length discovery scans until the first terminator, so the pointer must reference a readable terminated
///          sequence. This hidden alias hook performs no allocation.
template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type> print_alias_define(io_alias_t, basic_os_c_str<char_type> bas) noexcept
{
	auto ptr{bas.ptr};
	return {ptr, ::fast_io::cstr_len(ptr)};
}

/// @brief Resolves a known-size terminated OS-string wrapper to a borrowed scatter.
/// @details Exactly `n` code units are exposed; the hook trusts the wrapper's caller-supplied extent.
template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type>
print_alias_define(io_alias_t, basic_os_c_str_with_known_size<char_type> bas) noexcept
{
	return {bas.ptr, bas.n};
}

/// @brief Resolves a counted non-terminated OS string wrapper to a borrowed scatter.
/// @details The result exposes exactly `[ptr, ptr + n)` and does not inspect a terminator.
template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type>
print_alias_define(io_alias_t, basic_os_str_known_size_without_null_terminated<char_type> bas) noexcept
{
	return {bas.ptr, bas.n};
}

/// @brief Preserves the explicit lifetime contract of the non-owning C-string wrappers through aliasing.
/// @details These wrappers contain only a caller-supplied character pointer (and, where applicable, its extent).
///          `print_alias_define` neither materializes characters nor redirects the pointer to scratch storage, so a
///          retained scatter has exactly the same lifetime obligation as the wrapper itself. The opt-in is deliberately
///          attached to these explicit pointer-view types instead of to arbitrary contiguous ranges.
template <::std::integral char_type>
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, basic_os_c_str<char_type>>) noexcept
{
	return {};
}

/// @brief Preserves borrowed-scatter provenance for a known-size terminated OS string wrapper.
/// @details The alias result still points into caller-owned storage and therefore retains the wrapper's lifetime rules.
template <::std::integral char_type>
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, basic_os_c_str_with_known_size<char_type>>) noexcept
{
	return {};
}

/// @brief Preserves borrowed-scatter provenance for a counted non-terminated OS string wrapper.
/// @details The alias hook forwards the caller's storage directly, so the original lifetime requirement remains.
template <::std::integral char_type>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, basic_os_str_known_size_without_null_terminated<char_type>>) noexcept
{
	return {};
}

/// @brief Creates a bounded C-string view by searching at most `n` code units for a terminator.
/// @details The resulting length excludes the terminator. `ch` must address `n` readable code units unless an earlier
///          terminator is encountered; the returned view remains non-owning.
template <::std::integral char_type>
inline constexpr basic_os_str_known_size_without_null_terminated<char_type> os_c_str(char_type const *ch, ::std::size_t n) noexcept
{
	return {ch, ::fast_io::cstr_nlen(ch, n)};
}

/// @brief Creates a bounded C-string view from a character array.
/// @details At most `n - 1` elements are searched, treating the final array slot as the conventional terminator
///          position rather than part of the returned text.
template <::std::integral char_type, ::std::size_t n>
	requires(n != 0)
inline constexpr basic_os_str_known_size_without_null_terminated<char_type> os_c_str_carr(char_type const (&cstr)[n]) noexcept
{
	constexpr ::std::size_t nm1{static_cast<::std::size_t>(n - 1u)};
	return os_c_str(cstr, nm1);
}

/// @brief Creates a known-size null-terminated C-string wrapper without scanning.
/// @details The caller promises that the supplied size and termination contract are valid for downstream consumers.
template <::std::integral char_type>
inline constexpr basic_os_c_str_with_known_size<char_type> os_c_str_null_terminated(char_type const *ch, ::std::size_t n) noexcept
{
	return {ch, n};
}

/// @brief Creates a known-size C-string wrapper from an array's first `n - 1` elements.
/// @details The array is expected to use its last element as the terminator; no run-time verification is performed.
template <::std::integral char_type, ::std::size_t n>
	requires(n != 0)
inline constexpr basic_os_str_known_size_without_null_terminated<char_type> os_c_str_null_terminated_carr(char_type const (&cstr)[n]) noexcept
{
	constexpr ::std::size_t nm1{static_cast<::std::size_t>(n - 1u)};
	return os_c_str_null_terminated(cstr, nm1);
}

/// @brief Converts a character array into the smallest useful non-owning semantic scatter.
/// @details The final array element is excluded as a terminator. Empty text becomes `io_null`, one code unit becomes
///          `chvw_t`, short text carries static extent, and larger text uses an ordinary borrowed scatter.
template <::std::integral char_type, ::std::size_t n>
	requires(n != 0)
inline constexpr auto small_scatter(char_type const (&s)[n]) noexcept
{
	using no_const_char_type = ::std::remove_const_t<char_type>;
	constexpr ::std::size_t nm1{n - 1};
	constexpr ::std::size_t boundary{64}, boundaryp1{boundary + 1};
	if constexpr (n <= 1)
	{
		return ::fast_io::io_null;
	}
	else if constexpr (n == 2)
	{
		return manipulators::chvw_t<no_const_char_type>{*s};
	}
	else if constexpr (n < boundaryp1)
	{
		return ::fast_io::manipulators::static_scatter_t<no_const_char_type, nm1>{s};
	}
	else
	{
		// Array type and constness do not reveal the linker section or memory domain. A named array can reside in an
		// MMIO/non-cacheable section, so only an explicit proof wrapper may opt this raw descriptor into prefetching.
		return basic_io_scatter_t<no_const_char_type>{s, nm1};
	}
}

/// @brief Rejects a null pointer for the bounded C-string overload.
/// @details A readable character source is required before a bounded terminator search can be performed.
template <::std::integral T>
inline constexpr void os_c_str(decltype(nullptr), ::std::size_t) noexcept = delete;

/// @brief Creates a borrowed string view from a contiguous iterator pair.
/// @details `[first, last)` must be a valid range within one contiguous allocation. No terminator is required and no
///          characters are copied.
template <::std::contiguous_iterator Iter>
	requires ::std::integral<::std::iter_value_t<Iter>>
inline constexpr basic_io_scatter_t<::std::remove_cvref_t<::std::iter_value_t<Iter>>> strvw(Iter first,
																							Iter last) noexcept
{
	return {::std::to_address(first), static_cast<::std::size_t>(last - first)};
}

/// @brief Creates a borrowed string view over an integral contiguous range.
/// @details The returned scatter refers directly to the range's storage; the source must outlive its consumption.
template <::std::ranges::contiguous_range rg>
	requires ::std::integral<::std::ranges::range_value_t<rg>>
inline constexpr basic_io_scatter_t<::std::remove_cvref_t<::std::ranges::range_value_t<rg>>> strvw(rg &&r) noexcept
{
	return {::std::ranges::data(r), ::std::ranges::size(r)};
}

/// @brief Treats a contiguous integral range as a bounded C string.
/// @details Scanning stops at the first terminator or the range end. The result excludes the terminator and borrows the
///          source storage.
template <::std::ranges::contiguous_range rg>
	requires(::std::integral<::std::ranges::range_value_t<rg>>)
inline constexpr basic_os_str_known_size_without_null_terminated<::std::remove_cvref_t<::std::ranges::range_value_t<rg>>>
os_c_str(rg &&r) noexcept
{
	auto p{::std::ranges::data(r)};
	return {p, ::fast_io::cstr_nlen(p, ::std::ranges::size(r))};
}

/// @brief Exposes an enumeration's underlying integer value for numeric formatting.
/// @details No symbolic name lookup is attempted; signedness and width are exactly those of the enum's underlying type.
template <typename enumtype>
	requires(::std::is_enum_v<enumtype>)
inline constexpr ::std::underlying_type_t<enumtype> enum_int_view(enumtype enm) noexcept
{
	return static_cast<::std::underlying_type_t<enumtype>>(enm);
}

} // namespace manipulators

template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type>
print_scatter_define(io_reserve_type_t<char_type, basic_io_scatter_t<char_type>>,
					 basic_io_scatter_t<char_type> iosc) noexcept
{
	return iosc;
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>,
	manipulators::bounded_cstr_scatter_t<char_type, extent> value) noexcept
{
	return {value.base, value.len};
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t extent>
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

/// @brief Projects a provenance-carrying descriptor onto the existing scatter-print protocol.
/// @details The result is intentionally the raw descriptor expected by scatter consumers. Cacheability remains a
///          property of the producer type at policy selection time; it need not alter the descriptor ABI passed to
///          write or copy machinery.
template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>,
	basic_prfch_cacheable_io_scatter_t<char_type> iosc) noexcept
{
	return iosc.scatter();
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_borrowed_scatter_source(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>) noexcept
{
	// Like a raw scatter, this type borrows storage explicitly supplied by its caller.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_scatter_output_state_independent(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>) noexcept
{
	// Observing an already-materialized pointer/length pair cannot inspect a destination cursor.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_scatter_direct_print_equivalent(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>) noexcept
{
	// The descriptor's character range is its complete print semantics; no hidden formatting operation is bypassed.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_copy_stable_borrowed_source(
	io_reserve_type_t<char_type, basic_prfch_cacheable_io_scatter_t<char_type>>) noexcept
{
	// Copying this two-word view preserves the same external range and therefore does not retain producer identity.
	return {};
}

namespace details
{

template <typename T, ::std::size_t N>
struct my_constant_passer
{
	using value_type = T;
	static inline constexpr ::std::size_t value{N};
};

template <::std::integral char_type, ::std::size_t n>
inline constexpr my_constant_passer<char_type, n> compute_char_literal_array_type(char_type (&)[n]) noexcept
{
	// The result depends only on the reference type and its extent; no array
	// value is observed.  constexpr keeps this type proof usable in Clang 13,
	// whose pre-DR20 immediate-call rules reject an otherwise unknown reference
	// inside a requires-expression, while every consumer still obtains the same
	// compile-time `my_constant_passer` type.
	return {};
}

template <typename T>
concept printaliascarray = ::std::is_array_v<::std::remove_reference_t<T>> &&
						   ::std::integral<::std::remove_extent_t<::std::remove_cvref_t<T>>> &&
						   requires(T const &s) { ::fast_io::details::compute_char_literal_array_type(s); };

} // namespace details

template <typename T>
	requires(::fast_io::details::printaliascarray<T>)
inline constexpr auto print_alias_define(io_alias_t, T const &s) noexcept
{
	using constanttype = decltype(::fast_io::details::compute_char_literal_array_type(s));
	using char_type = typename constanttype::value_type;
	using no_const_char_type = ::std::remove_const_t<char_type>;

	constexpr bool not_char_literal{::std::is_const_v<char_type>};
	constexpr ::std::size_t n{constanttype::value};
	constexpr ::std::size_t nm1{n - 1};
	static_assert(n != 0);
	static_assert(not_char_literal, "The type is an array but not char array literal. Reject.");

	if constexpr (n <= 1)
	{
		return ::fast_io::io_null;
	}
	else if constexpr (n == 2)
	{
		return manipulators::chvw_t<no_const_char_type>{*s};
	}
	else if constexpr (n < 65)
	{
		return manipulators::static_scatter_t<no_const_char_type, nm1>{s};
	}
	else
	{
		return basic_io_scatter_t<no_const_char_type>{s, nm1};
	}
}

/// @brief Proves that a character-array alias borrows the array's own storage.
/// @details The long-array branch above returns a raw scatter, while its shorter branches use specialized proxy types.
///          In every branch the address is the original array address: no helper owns temporary characters and no
///          subsequent alias call can overwrite the array. An lvalue array remains owned by its caller, while an array
///          temporary bound for a print expression remains alive through that full expression; either lifetime encloses
///          the operation for which the descriptor may be retained. This lifetime proof is deliberately separate from
///          cacheability: the type system cannot distinguish literal rodata from a named const array in a special
///          MMIO/non-cacheable linker section.
template <::std::integral char_type, typename source_char_type, ::std::size_t n>
	requires(::std::same_as<char_type, ::std::remove_cv_t<source_char_type>>)
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, source_char_type[n]>) noexcept
{
	return {};
}

template <::std::integral char_type, typename source_char_type, ::std::size_t n>
	requires(::std::same_as<char_type, ::std::remove_cv_t<source_char_type>>)
inline constexpr ::std::true_type
print_eager_materialization_safe(io_reserve_type_t<char_type, source_char_type[n]>) noexcept
{
	return {};
}

template <typename T>
	requires(::std::ranges::contiguous_range<T> && requires(T &&t) { t.substr(); })
inline constexpr basic_io_scatter_t<::std::remove_cvref_t<::std::ranges::range_value_t<T>>>
print_alias_define(io_alias_t, T &&svw) noexcept
{
	return {::std::ranges::data(svw), ::std::ranges::size(svw)};
}

// Deliberately no generic `print_borrowed_scatter_source` accompanies this extension point. `contiguous_range` proves
// pointer arithmetic and `substr()` proves only an interface shape; neither states that a third-party view's `data()`
// is independent of mutable scratch reused by the next range element. Concrete standard and fast_io string/view types
// provide their own source-side proof where that stronger lifetime property is known.

template <::std::integral char_type, ::std::integral pchar_type>
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>) noexcept
{
	return 1;
}

template <::std::integral char_type, ::std::integral pchar_type>
inline constexpr ::std::size_t
print_reserve_static_precise_size(io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>) noexcept
{
	return 1;
}

template <::std::integral char_type, ::std::integral pchar_type, typename T>
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>,
												 char_type *iter, T ch) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	*iter = static_cast<char_type>(static_cast<unsigned_char_type>(ch.reference));
	return ++iter;
}

template <::std::integral char_type, ::std::integral pchar_type>
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>,
						   manipulators::chvw_t<pchar_type>) noexcept
{
	return 1;
}

template <::std::integral char_type, ::std::integral pchar_type>
inline constexpr char_type *
print_reserve_precise_define(io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>,
							 char_type *iter, ::std::size_t, manipulators::chvw_t<pchar_type> ch) noexcept
{
	return print_reserve_define(io_reserve_type<char_type, manipulators::chvw_t<pchar_type>>, iter, ch);
}

template <::std::integral char_type, ::std::integral pchar_type>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>) noexcept
{
	return N;
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::size_t
print_reserve_static_precise_size(io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>) noexcept
{
	return N;
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>,
					 char_type *iter, ::fast_io::manipulators::static_scatter_t<char_type, N> scatter) noexcept
{
	// A static scatter declares an exact extent contract, so the type-level small-copy policy may expose its individual
	// code units before GCC's loop-to-memcpy recognition. The helper reads and writes exactly N elements; an N-element
	// source with no trailing null is therefore sufficient and no over-copy is permitted.
	return ::fast_io::details::decay::static_scatter_copy_n<N>(scatter.base, iter);
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>,
						   ::fast_io::manipulators::static_scatter_t<char_type, N>) noexcept
{
	return N;
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr char_type *
print_reserve_precise_define(io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>,
							 char_type *iter, ::std::size_t,
							 ::fast_io::manipulators::static_scatter_t<char_type, N> scatter) noexcept
{
	// Precise concat and ordinary reserve output share one lowering policy; otherwise the same semantic leaf would regain
	// fragmented memcpy stores merely by crossing the destination concept boundary.
	return ::fast_io::details::decay::static_scatter_copy_n<N>(scatter.base, iter);
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, ::fast_io::manipulators::static_scatter_t<char_type, N>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::small_scatter_t<char_type, N>>) noexcept
{
	return N;
}

namespace details
{

template <::std::integral char_type>
inline constexpr char_type *small_scatter_print_reserve_define_impl(char_type *iter, char_type const *base,
																	::std::size_t len) noexcept
{
	return ::fast_io::details::non_overlapped_copy_n(base, len, iter);
}

} // namespace details

template <::std::integral char_type, ::std::size_t N>
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::small_scatter_t<char_type, N>>,
					 char_type *iter, ::fast_io::manipulators::small_scatter_t<char_type, N> scatter) noexcept
{
	scatter.validate();
	return ::fast_io::details::small_scatter_print_reserve_define_impl(iter, scatter.base, scatter.len);
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, ::fast_io::manipulators::small_scatter_t<char_type, N>>,
						   ::fast_io::manipulators::small_scatter_t<char_type, N> scatter) noexcept
{
	scatter.validate();
	return scatter.len;
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr char_type *
print_reserve_precise_define(io_reserve_type_t<char_type, ::fast_io::manipulators::small_scatter_t<char_type, N>>,
								 char_type *iter, ::std::size_t,
								 ::fast_io::manipulators::small_scatter_t<char_type, N> scatter) noexcept
{
	scatter.validate();
	return ::fast_io::details::small_scatter_print_reserve_define_impl(iter, scatter.base, scatter.len);
}

template <::std::integral char_type, ::std::size_t N>
inline constexpr ::std::true_type print_eager_materialization_safe(
	io_reserve_type_t<char_type, ::fast_io::manipulators::small_scatter_t<char_type, N>>) noexcept
{
	return {};
}

// Fixed reserve leaves are identity members of compiler-constant runs.  Their ordinary CPOs already use the compact
// copy shape needed by the materialized scalar proxy, so no second representation or formatter is introduced.
template <::std::integral char_type, ::std::integral pchar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::integral pchar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>) noexcept
{
	// This identity provider has no replacement graph: its materializer returns the same fixed reserve leaf.
	return {};
}

template <::std::integral char_type, ::std::integral pchar_type>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>,
	manipulators::chvw_t<pchar_type> const &) noexcept
{
	return true;
}

template <::std::integral char_type, ::std::integral pchar_type>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	io_reserve_type_t<char_type, manipulators::chvw_t<pchar_type>>,
	manipulators::chvw_t<pchar_type> const &value) noexcept
{
	return value;
}

namespace details
{

template <::std::integral char_type, ::std::size_t extent,
		  ::std::size_t... index>
[[nodiscard]] inline constexpr bool
bounded_cstr_compiler_constant_eligible_impl(
	manipulators::bounded_cstr_scatter_t<char_type, extent> const &value,
	::std::index_sequence<index...>) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.len) && value.len <= extent &&
		   ((index >= value.len || __builtin_constant_p(value.base[index])) && ...);
#else
	(void)value;
	return false;
#endif
}

} // namespace details

template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

/// @brief Lets a proved bounded C-string literal cross print's source-normalization boundary.
/// @details The replacement preserves exactly the already-measured `[base, base + len)` spelling and merely changes
///          its transport from a borrowed scatter to bounded reserve storage.  Unknown arrays fail the inline
///          compiler-constant query and therefore retain the historical scatter path without materialization work.
template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

/// @brief Records the bounded C-string provider's permanent field-by-field query classification.
/// @details The matrix proves both `len` and every admitted character independently unknown, and consumers still decide
///          whether copying the bounded spelling removes work for their concrete destination.
template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>,
	manipulators::bounded_cstr_scatter_t<char_type, extent> const &value) noexcept
{
	if constexpr (
		extent > ::fast_io::details::compiler_constant_materialization_max_bytes /
					 sizeof(char_type))
	{
		(void)value;
		return false;
	}
	else
	{
		return ::fast_io::details::bounded_cstr_compiler_constant_eligible_impl(
			value, ::std::make_index_sequence<extent>{});
	}
}

template <::std::integral char_type, ::std::size_t extent>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	io_reserve_type_t<
		char_type, manipulators::bounded_cstr_scatter_t<char_type, extent>>,
	manipulators::bounded_cstr_scatter_t<char_type, extent> const &value) noexcept
{
	return manipulators::small_scatter_t<char_type, extent>{
		value.base, value.len};
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type, manipulators::static_scatter_t<char_type, N>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type, manipulators::static_scatter_t<char_type, N>>) noexcept
{
	// Static scatters are identity providers whose payload already has static lifetime.
	return {};
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	io_reserve_type_t<char_type, manipulators::static_scatter_t<char_type, N>>,
	manipulators::static_scatter_t<char_type, N> const &) noexcept
{
	return true;
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	io_reserve_type_t<char_type, manipulators::static_scatter_t<char_type, N>>,
	manipulators::static_scatter_t<char_type, N> const &value) noexcept
{
	return value;
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	io_reserve_type_t<
		char_type, manipulators::small_scatter_t<char_type, N>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	io_reserve_type_t<
		char_type, manipulators::small_scatter_t<char_type, N>>) noexcept
{
	// The bounded small-scatter materializer is also an identity operation; consumer size gates remain unchanged.
	return {};
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	io_reserve_type_t<char_type, manipulators::small_scatter_t<char_type, N>>,
	manipulators::small_scatter_t<char_type, N> const &) noexcept
{
	return true;
}

template <::std::integral char_type, ::std::size_t N>
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	io_reserve_type_t<char_type, manipulators::small_scatter_t<char_type, N>>,
	manipulators::small_scatter_t<char_type, N> const &value) noexcept
{
	return value;
}

} // namespace fast_io
