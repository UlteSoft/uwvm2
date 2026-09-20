#pragma once

/*
 * Normalized input-device capability matrix (CPO level).
 *
 * Against an input observer that has already crossed `input_stream_ref`, this
 * file recognizes coherent ibuffer cursors, whole-record `status_scan_define`,
 * typed and byte reads, sequential and positioned reads, some/all variants,
 * and contiguous/scatter forms. Composite `readable` concepts summarize which
 * primitive synthesis paths are available. No target scanning or refill loop
 * is executed here; scan/read algorithms consume these exact capabilities.
 */

namespace fast_io
{

namespace operations::decay::defines
{

/// @brief Proves the coherent cursor protocol required by buffered scan dispatch.
/// @details Read-only views legitimately expose `char_type const*`, while refill buffers expose `char_type*`; requiring
///          either spelling globally would reject one valid family. Instead, the proof requires all three cursors to
///          have one exact pointer type whose unqualified pointee is `input_char_type`, then passes that discovered type
///          back to `ibuffer_set_curr`. This separation admits both ownership models without allowing a concept-positive
///          stream whose real cursor cannot be committed. The exact bool underflow result is the progress/EOF contract.
template <typename T>
concept has_ibuffer_basic_operations = requires(T instm) {
	requires ::std::is_pointer_v<decltype(ibuffer_curr(instm))>;
	requires ::std::same_as<decltype(ibuffer_begin(instm)), decltype(ibuffer_curr(instm))>;
	requires ::std::same_as<decltype(ibuffer_curr(instm)), decltype(ibuffer_end(instm))>;
	requires ::std::same_as<
		::std::remove_cv_t<::std::remove_pointer_t<decltype(ibuffer_curr(instm))>>,
		typename decltype(instm)::input_char_type>;
	requires(!::std::is_volatile_v<
		::std::remove_pointer_t<decltype(ibuffer_curr(instm))>>);
	{
		ibuffer_set_curr(instm, ibuffer_curr(instm))
	} -> ::std::same_as<void>;
	{ ibuffer_underflow(instm) } -> ::std::same_as<bool>;
};

/// @brief Detects the exact status-scan expression that the dispatcher will invoke.
/// @details The former probe hard-coded an integer and a `<true>` template argument, while dispatch called an
///          unqualified non-templated expression with the actual scanner pack. A stream specialized for a textual
///          scanner could therefore be rejected, and a stream satisfying only the probe could still fail in the body.
///          Parameterizing the concept by Args makes recognition and invocation the same expression.
template <typename T, typename... Args>
concept has_status_scan_define = requires(T optstm, Args... args) {
	{ status_scan_define(optstm, args...) } -> ::std::same_as<bool>;
};

// These result constraints are part of the primitive protocol. Scalar `some` returns the first unfilled pointer,
// scatter `some` returns a descriptor-relative status, and `all` returns void because successful return itself proves
// completion. Keeping that distinction in capability recognition prevents invalid customizations from selecting a
// strategy whose body later attempts pointer arithmetic or structured binding on an unrelated result type.
template <typename T>
concept has_read_some_underflow_define = requires(T instm, typename decltype(instm)::input_char_type *ptr) {
	{ read_some_underflow_define(instm, ptr, ptr) } -> ::std::same_as<typename decltype(instm)::input_char_type *>;
};

template <typename T>
concept has_read_all_underflow_define = requires(T instm, typename decltype(instm)::input_char_type *ptr) {
	{ read_all_underflow_define(instm, ptr, ptr) } -> ::std::same_as<void>;
};

template <typename T>
concept has_read_some_bytes_underflow_define = requires(T instm, ::std::byte *ptr) {
	{ read_some_bytes_underflow_define(instm, ptr, ptr) } -> ::std::same_as<::std::byte *>;
};

template <typename T>
concept has_read_all_bytes_underflow_define = requires(T instm, ::std::byte *ptr) {
	{ read_all_bytes_underflow_define(instm, ptr, ptr) } -> ::std::same_as<void>;
};

template <typename T>
concept has_scatter_read_some_bytes_underflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_read_some_bytes_underflow_define(instm, scatter, len) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_read_all_bytes_underflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_read_all_bytes_underflow_define(instm, scatter, len) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_scatter_read_some_underflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::input_char_type> const *pscatter,
			 ::std::size_t len) {
		{ scatter_read_some_underflow_define(instm, pscatter, len) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_read_all_underflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::input_char_type> const *pscatter,
			 ::std::size_t len) {
		{ scatter_read_all_underflow_define(instm, pscatter, len) } -> ::std::same_as<void>;
	};

/// @brief Recognizes input buffers that can report whether their current end is terminal.
/// @details The query is made on the already-decayed input observer. The old probe misspelled the customization
///          and routed the object through `output_stream_ref`, making every existing buffer-view and memory-map marker
///          invisible. Terminal knowledge lets hybrid scanners accept a successful parse ending exactly at `end`
///          without a speculative second parse. The bool value, rather than CPO presence, is the assertion: a backend
///          may use one observer type for run-time terminal and refillable modes. The exact result type keeps an
///          unrelated query with the same spelling from becoming a capability.
template <typename T>
concept has_ibuffer_underflow_never_define = requires(T instm) {
	{
		ibuffer_underflow_never(instm)
	} -> ::std::same_as<bool>;
};

template <typename T>
concept has_pread_some_bytes_underflow_define =
	requires(T instm, ::std::byte *ptr) {
		{ pread_some_bytes_underflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<::std::byte *>;
	};

template <typename T>
concept has_pread_all_bytes_underflow_define =
	requires(T instm, ::std::byte *ptr) {
		{ pread_all_bytes_underflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_scatter_pread_some_bytes_underflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_pread_some_bytes_underflow_define(instm, scatter, len, 0) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_pread_all_bytes_underflow_define =
	requires(T instm, ::fast_io::io_scatter_t const *scatter, ::std::size_t len) {
		{ scatter_pread_all_bytes_underflow_define(instm, scatter, len, 0) } -> ::std::same_as<void>;
	};

template <typename T>
concept has_pread_some_underflow_define = requires(T instm, typename decltype(instm)::input_char_type *ptr) {
	{ pread_some_underflow_define(instm, ptr, ptr, 0) }
		-> ::std::same_as<typename decltype(instm)::input_char_type *>;
};

template <typename T>
concept has_pread_all_underflow_define = requires(T instm, typename decltype(instm)::input_char_type *ptr) {
	{ pread_all_underflow_define(instm, ptr, ptr, 0) } -> ::std::same_as<void>;
};

template <typename T>
concept has_scatter_pread_some_underflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::input_char_type> const *scatter,
			 ::std::size_t len) {
		{ scatter_pread_some_underflow_define(instm, scatter, len, 0) }
			-> ::std::same_as<::fast_io::io_scatter_status_t>;
	};

template <typename T>
concept has_scatter_pread_all_underflow_define =
	requires(T instm, ::fast_io::basic_io_scatter_t<typename decltype(instm)::input_char_type> const *scatter,
			 ::std::size_t len) {
		{ scatter_pread_all_underflow_define(instm, scatter, len, 0) } -> ::std::same_as<void>;
	};

template <typename stmtype>
concept has_any_of_read_bytes_operations =
	::fast_io::operations::decay::defines::has_read_some_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_read_all_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_read_some_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_read_all_bytes_underflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_read_operations =
	::fast_io::operations::decay::defines::has_read_some_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_read_all_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_read_some_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_read_all_underflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_pread_bytes_operations =
	::fast_io::operations::decay::defines::has_pread_some_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_pread_all_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pread_some_bytes_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pread_all_bytes_underflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_pread_operations =
	::fast_io::operations::decay::defines::has_pread_some_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_pread_all_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pread_some_underflow_define<stmtype> ||
	::fast_io::operations::decay::defines::has_scatter_pread_all_underflow_define<stmtype>;

template <typename stmtype>
concept has_any_of_read_or_seek_pread_bytes_operations =
	::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_input_stream_seek_bytes_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<stmtype>);

template <typename stmtype>
concept has_any_of_pread_or_seek_read_bytes_operations =
	::fast_io::operations::decay::defines::has_any_of_pread_bytes_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_input_stream_seek_bytes_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_read_bytes_operations<stmtype>);

template <typename stmtype>
concept has_any_of_read_or_seek_pread_operations =
	::fast_io::operations::decay::defines::has_any_of_read_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_input_stream_seek_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_pread_operations<stmtype>);

template <typename stmtype>
concept has_any_of_pread_or_seek_read_operations =
	::fast_io::operations::decay::defines::has_any_of_pread_operations<stmtype> ||
	(::fast_io::operations::decay::defines::has_input_stream_seek_define<stmtype> &&
	 ::fast_io::operations::decay::defines::has_any_of_read_operations<stmtype>);

template <typename stmtype>
concept readable =
	has_any_of_read_or_seek_pread_operations<stmtype> ||
	(sizeof(typename stmtype::input_char_type) == 1 && has_any_of_read_or_seek_pread_bytes_operations<stmtype>);

template <typename stmtype>
concept bytes_readable =
	has_any_of_read_or_seek_pread_bytes_operations<stmtype> ||
	(sizeof(typename stmtype::input_char_type) == 1 && has_any_of_read_or_seek_pread_operations<stmtype>);

template <typename stmtype>
concept preadable =
	has_any_of_pread_or_seek_read_operations<stmtype> ||
	(sizeof(typename stmtype::input_char_type) == 1 && has_any_of_pread_or_seek_read_bytes_operations<stmtype>);

template <typename stmtype>
concept bytes_preadable =
	has_any_of_pread_or_seek_read_bytes_operations<stmtype> ||
	(sizeof(typename stmtype::input_char_type) == 1 && has_any_of_pread_or_seek_read_operations<stmtype>);

/// @brief Recognizes a compile-time minimum input-window guarantee and its matching refill operation.
/// @details Consumers use the reported value as a non-type constant before an input object exists. Exact `size_t`
///          return typing alone still admits a run-time CPO and fails later in a variable template. Forming
///          `compile_time_size_constant` proves constant evaluation; nonzero and pointer-difference bounds prove that
///          the value describes one useful C++ buffer range. The prepare CPO returns exactly void because completion
///          is represented solely by the subsequently queried begin/current/end cursor state.
template <typename T>
concept has_ibuffer_minimum_size_operations =
	requires {
		typename ::fast_io::details::compile_time_size_constant<ibuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::input_char_type, T>)>;
		{
			ibuffer_minimum_size_define(::fast_io::io_reserve_type<typename T::input_char_type, T>)
		} -> ::std::same_as<::std::size_t>;
		requires(ibuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::input_char_type, T>) != 0u);
		requires(ibuffer_minimum_size_define(
			::fast_io::io_reserve_type<typename T::input_char_type, T>) <
			 static_cast<::std::size_t>(PTRDIFF_MAX));
	} &&
	requires(T instm) {
		{
			ibuffer_minimum_size_underflow_all_prepare_define(instm)
		} -> ::std::same_as<void>;
	};

} // namespace operations::decay::defines

} // namespace fast_io
