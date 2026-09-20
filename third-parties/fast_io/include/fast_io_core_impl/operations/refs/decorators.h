#pragma once

/*
 * Directional decorator-reference normalization (CPO level).
 *
 * These CPOs project a stream to the input, output, or bidirectional decorator
 * object used by transformation algorithms. Storage follows the same
 * borrow-or-own discipline as stream references: stable mutable lvalues may be
 * borrowed and proxy prvalues are materialized once. This file selects the
 * decorator object only; it does not execute a transcode or device operation.
 */

namespace fast_io
{

namespace operations::defines
{

/// @brief Proves that a decorator-reference CPO result can survive one normalization expression.
/// @details Mutable lvalues are already stable storage. A prvalue obtains its local owner through guaranteed copy
///          elision, while const lvalues and xvalues must actually construct a value. Reusing the stream-reference
///          storage proof keeps incomplete and void CPO results substitution failures instead of body diagnostics;
///          character domains are intentionally absent because decorator direction determines them later.
template <typename result_type>
concept storable_decorators_ref_result =
	::fast_io::operations::defines::storable_stream_ref_result_object<result_type>();

/// @brief Borrows only a mutable lvalue decorator projection with CPO-supplied storage.
/// @details Decoder/encoder objects commonly retain partial-sequence state, so even a small trivially copyable lvalue is
///          not presumed identity-independent.  Prvalues, xvalues, and cv-qualified lvalues are materialized once by the
///          reference helper; the storable-result proof above checks the exact construction before this branch is formed.
template <typename result_type>
inline constexpr bool decorators_ref_result_borrows_lvalue =
	::std::is_lvalue_reference_v<result_type> &&
	!::std::is_const_v<::std::remove_reference_t<result_type>> &&
	!::std::is_volatile_v<::std::remove_reference_t<result_type>>;

template <typename T>
concept has_input_decorators_ref_define = requires(T &&t) {
	input_decorators_ref_define(t);
	requires ::fast_io::operations::defines::storable_decorators_ref_result<
		decltype(input_decorators_ref_define(t))>;
};

template <typename T>
concept has_output_decorators_ref_define = requires(T &&t) {
	output_decorators_ref_define(t);
	requires ::fast_io::operations::defines::storable_decorators_ref_result<
		decltype(output_decorators_ref_define(t))>;
};

template <typename T>
concept has_io_decorators_ref_define = requires(T &&t) {
	io_decorators_ref_define(t);
	requires ::fast_io::operations::defines::storable_decorators_ref_result<
		decltype(io_decorators_ref_define(t))>;
};

template <typename T>
concept has_input_or_io_decorators_ref_define = has_input_decorators_ref_define<T> || has_io_decorators_ref_define<T>;

template <typename T>
concept has_output_or_io_decorators_ref_define = has_output_decorators_ref_define<T> || has_io_decorators_ref_define<T>;

} // namespace operations::defines

namespace operations::refs
{

template <typename T>
	requires ::fast_io::operations::defines::has_input_or_io_decorators_ref_define<T>
inline constexpr decltype(auto) input_decorators_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_input_decorators_ref_define<T>)
	{
		using result_type = decltype(input_decorators_ref_define(t));
		if constexpr (::fast_io::operations::defines::decorators_ref_result_borrows_lvalue<result_type>)
		{
			return input_decorators_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(input_decorators_ref_define(t));
		}
	}
	else
	{
		using result_type = decltype(io_decorators_ref_define(t));
		if constexpr (::fast_io::operations::defines::decorators_ref_result_borrows_lvalue<result_type>)
		{
			return io_decorators_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_decorators_ref_define(t));
		}
	}
}

template <typename T>
	requires ::fast_io::operations::defines::has_output_or_io_decorators_ref_define<T>
inline constexpr decltype(auto) output_decorators_ref(T &&t)
{
	if constexpr (::fast_io::operations::defines::has_output_decorators_ref_define<T>)
	{
		using result_type = decltype(output_decorators_ref_define(t));
		if constexpr (::fast_io::operations::defines::decorators_ref_result_borrows_lvalue<result_type>)
		{
			return output_decorators_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(output_decorators_ref_define(t));
		}
	}
	else
	{
		using result_type = decltype(io_decorators_ref_define(t));
		if constexpr (::fast_io::operations::defines::decorators_ref_result_borrows_lvalue<result_type>)
		{
			return io_decorators_ref_define(t);
		}
		else
		{
			return ::std::remove_cvref_t<result_type>(io_decorators_ref_define(t));
		}
	}
}

template <typename T>
	requires ::fast_io::operations::defines::has_io_decorators_ref_define<T>
inline constexpr decltype(auto) io_decorators_ref(T &&t)
{
	using result_type = decltype(io_decorators_ref_define(t));
	if constexpr (::fast_io::operations::defines::decorators_ref_result_borrows_lvalue<result_type>)
	{
		return io_decorators_ref_define(t);
	}
	else
	{
		return ::std::remove_cvref_t<result_type>(io_decorators_ref_define(t));
	}
}

} // namespace operations::refs

} // namespace fast_io
