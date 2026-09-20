#pragma once

/**
 * @file
 * @brief Defines the public output-operation lifetime boundary.
 *
 * A provider may explicitly opt an owning output type into checked terminal
 * finish. Only an rvalue/xvalue of that exact owner activates the policy;
 * lvalues and borrowed refs merely share the same one-time normalization path.
 * Forwarding facades may name the output as an lvalue while retaining its
 * original lifetime category in the second template argument. The guard has
 * no destructor action: callers invoke commit only after their operation body
 * succeeds, so unwinding can never hide a finish failure.
 */

namespace fast_io
{

/** @brief Opt-in policy for checked finish of temporary output owners. */
template <typename T>
inline constexpr bool temporary_output_finish_enabled = false;

namespace operations
{

/**
 * @brief Owns one normalized output observer for a complete public operation.
 *
 * `public_output` describes how the facade presents the object to stream-ref
 * selection; `lifetime_output` preserves the original caller category when an
 * intermediate callback has named that object as an lvalue.
 */
template <typename public_output, typename lifetime_output = public_output>
class basic_output_operation_guard
{
public:
	using output_type = ::std::remove_reference_t<public_output>;
	using normalized_ref_type = decltype(::fast_io::operations::output_stream_ref(
		::std::declval<output_type &>()));

	static inline constexpr bool checked_finish{
		!::std::is_lvalue_reference_v<lifetime_output> &&
		::fast_io::temporary_output_finish_enabled<
			::std::remove_cvref_t<lifetime_output>>};

private:
	normalized_ref_type normalized_ref;

public:
	/** @brief Normalizes the public output exactly once for the operation. */
	inline explicit constexpr basic_output_operation_guard(output_type &output) noexcept(noexcept(::fast_io::operations::output_stream_ref(output)))
		: normalized_ref(::fast_io::operations::output_stream_ref(output))
	{}

	/** @brief Prevents a second guard from committing the same operation. */
	basic_output_operation_guard(basic_output_operation_guard const &) = delete;
	/** @brief Prevents reassignment from duplicating checked-finish ownership. */
	basic_output_operation_guard &operator=(basic_output_operation_guard const &) = delete;

	/** @brief Returns the stable normalized observer used by the operation body. */
	inline constexpr decltype(auto) ref() noexcept
	{
		return (normalized_ref);
	}

	/** @brief Performs checked terminal finish after a successful operation. */
	inline constexpr void commit()
	{
		// Commit is intentionally explicit and has no destructor fallback. It is
		// reached only after the operation body succeeds, so a finish exception
		// remains observable and never replaces an exception already unwinding.
		if constexpr (checked_finish)
		{
			// Finish only opted-in temporary owners; borrowed streams remain open.
			static_assert(requires(normalized_ref_type &ref_value) {
				{ output_stream_finish_define(ref_value) } -> ::std::same_as<void>;
			});
			output_stream_finish_define(normalized_ref);
		}
	}
};

/** @brief Invokes an operation and commits its guard only after success. */
template <typename guard_type, typename operation>
inline constexpr decltype(auto) output_operation_guard_invoke(
	guard_type &guard, operation &&body)
{
	// Preserve both void and value-returning public APIs while imposing the same
	// success-then-finish ordering. The result is captured before finish so its
	// exact decltype(auto) category remains unchanged.
	if constexpr (::std::is_void_v<decltype(body(guard.ref()))>)
	{
		// Void operations commit immediately after their body returns normally.
		body(guard.ref());
		guard.commit();
	}
	else
	{
		// Preserve a non-void result before the potentially throwing finish step.
		decltype(auto) result{body(guard.ref())};
		guard.commit();
		return result;
	}
}

} // namespace operations

} // namespace fast_io
