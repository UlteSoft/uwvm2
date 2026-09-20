#pragma once

/*
 * Normalized output-device capability matrix (CPO level).
 *
 * Against an output observer that has already crossed `output_stream_ref`,
 * this file recognizes coherent obuffer cursors, whole-record
 * `status_print_define`, typed and byte writes, sequential and positioned
 * writes, some/all variants, contiguous/scatter forms, line-buffering, and
 * reserve/flush hooks. Composite writable concepts describe available
 * primitive synthesis paths. Formatting and complete-record strategy are
 * selected by the IO print engine, not by these probes.
 */

namespace fast_io
{

/// @brief Seeds explicit-template-id parsing without supplying a viable status-print operation.
/// @details MSVC 19.35 and 19.36 reject a dependent `status_print_define<line>(...)` before late ADL when ordinary
///          lookup has not yet found a template; 19.34 and 19.37 accept the standard expression. This deleted
///          zero-parameter declaration gives those two frontends a template-name while remaining non-viable for every
///          real call, which always passes an output object. Provider overloads are still discovered and selected
///          exclusively by ADL, so capability identity and generated code are unchanged.
template <bool>
void status_print_define() = delete;

namespace operations::decay::defines
{

/// @brief Proves the complete mutable output-cursor protocol used by buffered print dispatch.
/// @details The three CPOs are constrained separately because each result participates in pointer subtraction or
///          assignment at a different call site; mere callability admits proxy and integer results that fail only
///          after the buffered strategy is selected. Reusing `obuffer_curr(instm)` as the setter argument proves that
///          discovery and commit use one exact pointer representation, while a void setter prevents an accidental
///          progress-query overload from masquerading as a cursor commit.
template <typename T>
concept has_obuffer_basic_operations = requires(T instm) {
	{
		obuffer_begin(instm)
	} -> ::std::same_as<typename decltype(instm)::output_char_type *>;
	{
		obuffer_curr(instm)
	} -> ::std::same_as<typename decltype(instm)::output_char_type *>;
	{
		obuffer_end(instm)
	} -> ::std::same_as<typename decltype(instm)::output_char_type *>;
	{
		obuffer_set_curr(instm, obuffer_curr(instm))
	} -> ::std::same_as<void>;
};

/// Detects the exact status-print expression used by the selected print operation.
///
/// A fixed probe such as `status_print_define<true>(stream, 0)` is not a valid
/// capability test: the customization is allowed to depend on both newline
/// ownership and the normalized argument types.  Such a probe can therefore
/// accept a stream for an expression that the dispatcher cannot call, or hide a
/// valid customization and incorrectly fall through to primitive I/O.  Keep the
/// complete expression in the concept identity so overload resolution here and
/// at the call site are provably the same operation. Status printing is a completion
/// operation, so its result is exactly void; accepting an unrelated value would make
/// this branch disagree with every ordinary print strategy.
///
/// For an empty `Args...` pack, `status_print_define<line>(stream)` is also the
/// provider's explicit opt-in to observing an otherwise empty logical record.
/// Without that exact expression the non-line dispatcher performs no primitive
/// output. Frontends which lower to an empty component pack must still enter the
/// IO dispatcher so this destination-owned decision is neither invented nor lost.
template <bool line, typename T, typename... Args>
concept has_status_print_define = requires(T optstm, Args... args) {
	{
		status_print_define<line>(optstm, args...)
	} -> ::std::same_as<void>;
};

// Primitive capability recognition deliberately constrains the result, rather than merely checking that a name is
// callable.  The decay dispatcher uses a pointer as progress for scalar `some`, io_scatter_status_t for scatter
// `some`, and no result at all for every `all` operation.  Accepting a customization with any other result postpones
// the error until a distant fallback is instantiated; more importantly, accepting a value-returning `all` operation
// would let strategy code accidentally treat an operation that promises completion as a progress query.
template <typename T>
concept has_write_some_overflow_define = requires(T instm, typename decltype(instm)::output_char_type const *ptr) {
	{ write_some_overflow_define(instm, ptr, ptr) }
		-> ::std::same_as<typename decltype(instm)::output_char_type const *>;
};

template <typename T>
concept has_write_all_overflow_define = requires(T instm, typename decltype(instm)::output_char_type const *ptr) {
	{ write_all_overflow_define(instm, ptr, ptr) } -> ::std::same_as<void>;
};

template <typename T>
concept has_write_some_bytes_overflow_define = requires(T instm, ::std::byte const *ptr) {
	{ write_some_bytes_overflow_define(instm, ptr, ptr) } -> ::std::same_as<::std::byte const *>;
};

template <typename T>
concept has_write_all_bytes_overflow_define = requires(T instm, ::std::byte const *ptr) {
	{ write_all_bytes_overflow_define(instm, ptr, ptr) } -> ::std::same_as<void>;
};

template <typename T>
concept has_scatter_write_some_bytes_overflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_write_some_bytes_overflow_define(instm, scatter, len) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_write_all_bytes_overflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_write_all_bytes_overflow_define(instm, scatter, len) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_scatter_write_some_overflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::output_char_type> const *pscatter,
			 ::std::size_t len) {
		{ scatter_write_some_overflow_define(instm, pscatter, len) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_write_all_overflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::output_char_type> const *pscatter,
			 ::std::size_t len) {
		{ scatter_write_all_overflow_define(instm, pscatter, len) } -> ::std::same_as<void>;
	};

/// @brief Recognizes a stream assertion that its current output extent cannot overflow.
/// @details Presence is the semantic marker; the exact bool result excludes unrelated status/count queries whose
///          spelling happens to match. Dispatch may use the assertion without inspecting an object-dependent value.
template <typename T>
concept has_obuffer_overflow_never_define = requires(T instm) {
	{
		obuffer_overflow_never(instm)
	} -> ::std::same_as<bool>;
};

template <typename T>
concept has_output_stream_char_put_overflow_define =
	requires(T instm, typename T::output_char_type ch) {
		{ output_stream_char_put_overflow_define(instm, ch) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_pwrite_some_bytes_overflow_define = requires(T instm, ::std::byte const *ptr) {
	{ pwrite_some_bytes_overflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<::std::byte const *>;
};

template <typename T>
concept has_pwrite_all_bytes_overflow_define = requires(T instm, ::std::byte const *ptr) {
	{ pwrite_all_bytes_overflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<void>;
};

template <typename T>
concept has_scatter_pwrite_some_bytes_overflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_pwrite_some_bytes_overflow_define(instm, scatter, len, 0) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_pwrite_all_bytes_overflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_pwrite_all_bytes_overflow_define(instm, scatter, len, 0) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_pwrite_some_overflow_define = requires(T instm, typename decltype(instm)::output_char_type const *ptr) {
	{ pwrite_some_overflow_define(instm, ptr, ptr, 0) }
		-> ::std::same_as<typename decltype(instm)::output_char_type const *>;
};

template <typename T>
concept has_pwrite_all_overflow_define = requires(T instm, typename decltype(instm)::output_char_type const *ptr) {
	{ pwrite_all_overflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<void>;
};


template <typename T>
concept has_scatter_pwrite_some_overflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::output_char_type> const *scatter,
			 ::std::size_t len) {
		{ scatter_pwrite_some_overflow_define(instm, scatter, len, 0) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_pwrite_all_overflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::output_char_type> const *scatter,
			 ::std::size_t len) {
		{ scatter_pwrite_all_overflow_define(instm, scatter, len, 0) } -> ::std::same_as<void>;
	};


/// @brief Recognizes the exact run-time line-buffering query protocol.
/// @details The current write consumers deliberately use the query's existence as a behavioral type marker: such a
///          stream may expose transient cursor/end relationships and therefore receives defensive range handling.
///          They do not interpret the returned value as a compile-time `true`. Requiring exactly `bool` still matters;
///          accepting an arbitrary callable (for example one returning an integer tag or `void`) would let an unrelated
///          overload opt the stream into that cursor policy accidentally.
template <typename T>
concept has_obuffer_is_line_buffering_define = requires(T outstm) {
	{ obuffer_is_line_buffering_define(outstm) } -> ::std::same_as<bool>;
};

template <typename stmtype>
concept has_any_of_write_bytes_operations =
	::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_write_operations =
	::fast_io::operations::decay::defines::has_write_some_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_write_all_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_pwrite_bytes_operations =
	::fast_io::operations::decay::defines::has_pwrite_some_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_pwrite_all_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pwrite_some_bytes_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pwrite_all_bytes_overflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_pwrite_operations =
	::fast_io::operations::decay::defines::has_pwrite_some_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_pwrite_all_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pwrite_some_overflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pwrite_all_overflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_write_or_seek_pwrite_bytes_operations =
	::fast_io::operations::decay::defines::has_any_of_write_bytes_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_output_stream_seek_bytes_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_pwrite_bytes_operations<stmtype>);

template <typename stmtype>
concept has_any_of_pwrite_or_seek_write_bytes_operations =
	::fast_io::operations::decay::defines::has_any_of_pwrite_bytes_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_output_stream_seek_bytes_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_write_bytes_operations<stmtype>);

template <typename stmtype>
concept has_any_of_write_or_seek_pwrite_operations =
	::fast_io::operations::decay::defines::has_any_of_write_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_output_stream_seek_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_pwrite_operations<stmtype>);

template <typename stmtype>
concept has_any_of_pwrite_or_seek_write_operations =
	::fast_io::operations::decay::defines::has_any_of_pwrite_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_output_stream_seek_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_write_operations<stmtype>);

template <typename stmtype>
concept writable =
	has_any_of_write_or_seek_pwrite_operations<stmtype> ||
	(sizeof(typename stmtype::output_char_type) == 1 && has_any_of_write_or_seek_pwrite_bytes_operations<stmtype>);

template <typename stmtype>
concept bytes_writable =
	has_any_of_write_or_seek_pwrite_bytes_operations<stmtype> ||
	(sizeof(typename stmtype::output_char_type) == 1 && has_any_of_write_or_seek_pwrite_operations<stmtype>);

template <typename stmtype>
concept pwritable =
	has_any_of_pwrite_or_seek_write_operations<stmtype> ||
	(sizeof(typename stmtype::output_char_type) == 1 && has_any_of_pwrite_or_seek_write_bytes_operations<stmtype>);

template <typename stmtype>
concept bytes_pwritable =
	has_any_of_pwrite_or_seek_write_bytes_operations<stmtype> ||
	(sizeof(typename stmtype::output_char_type) == 1 && has_any_of_pwrite_or_seek_write_operations<stmtype>);

/// @brief Recognizes a compile-time minimum-capacity guarantee and its matching refill operation.
/// @details The print cost model uses the advertised size as a non-type constant before any stream object exists.
///          Checking only an exact return type still admits a run-time function and later produces a hard error in a
///          variable template. Forming `compile_time_size_constant` is the SFINAE-friendly constant-expression proof.
///          A zero guarantee cannot enable a fast path, while values outside the pointer-difference domain cannot
///          describe one valid C++ put area. The refill operation returns `void` because its only result is the new
///          begin/current/end state queried after the call.
template <typename T>
concept has_obuffer_minimum_size_operations =
	requires {
		typename ::fast_io::details::compile_time_size_constant<obuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::output_char_type, T>)>;
		{
			obuffer_minimum_size_define(::fast_io::io_reserve_type<typename T::output_char_type, T>)
		} -> ::std::same_as<::std::size_t>;
		requires(obuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::output_char_type, T>) != 0u);
		requires(obuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::output_char_type, T>) <
			 static_cast<::std::size_t>(PTRDIFF_MAX));
	} &&
	requires(T outstm) {
		{ obuffer_minimum_size_flush_prepare_define(outstm) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_obuffer_flush_reserve_define = requires(T outstm, ::std::size_t to_reserve) {
	{ obuffer_flush_reserve_define(outstm, to_reserve) } -> ::std::same_as<void>;
};

} // namespace operations::decay::defines

} // namespace fast_io
