#pragma once

// Shared storage for incremental scan and print states. The historical file name is retained so existing internal
// include order remains stable; protocol tags below keep scan and print policy specializations ODR-distinct.

namespace fast_io::details
{

/// @brief Distinguishes scan-state storage specializations from print-state storage specializations.
/// @details The tag is part of helper identity even when both protocols advertise the same C++ state type. This avoids
///          cross-protocol COMDAT folding from erasing a deliberately distinct policy decision.
struct scan_context_state_storage_tag
{};

/// @brief Gives print-state helpers their own ODR-visible policy identity.
struct print_context_state_storage_tag
{};

/// @brief Returns the maximum bytes assigned to one inline incremental-protocol state.
/// @details Context state shares the configured print/scan stack budget, but a hard 4-KiB ceiling prevents a producer
///          or scanner type from silently inflating every caller's hot frame. Zero in the configured policy disables
///          inline state.
template <typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t context_inline_state_budget_bytes() noexcept
{
	constexpr ::std::size_t configured_bytes{
		::fast_io::details::print_stack_buffer_max_bytes<stack_policy>()};
	constexpr ::std::size_t preferred_bytes{4096u};
	return configured_bytes < preferred_bytes ? configured_bytes : preferred_bytes;
}

/// @brief Proves that one state slot, including worst-case alignment padding, fits the inline budget.
/// @details `sizeof(state_type) <= budget` alone is insufficient for an over-aligned type: realigning the frame can
///          consume up to `alignof(state_type)-1` additional bytes. The subtraction is reached only after proving that
///          alignment itself fits, so the predicate is free of unsigned underflow.
template <typename state_type, typename stack_policy = ::fast_io::details::default_print_stack_policy,
		  typename storage_identity = void>
inline constexpr bool context_state_inline_v = [] {
	constexpr ::std::size_t budget{
		::fast_io::details::context_inline_state_budget_bytes<stack_policy>()};
	if constexpr (budget == 0u || alignof(state_type) > budget)
	{
		return false;
	}
	else
	{
		return sizeof(state_type) <= budget - (alignof(state_type) - 1u);
	}
}();

template <typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t scan_context_inline_state_budget_bytes() noexcept
{
	return ::fast_io::details::context_inline_state_budget_bytes<stack_policy>();
}

template <typename state_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr bool scan_context_state_inline_v = ::fast_io::details::context_state_inline_v<
	state_type, stack_policy, ::fast_io::details::scan_context_state_storage_tag>;

template <typename state_type, typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr bool print_context_state_inline_v = ::fast_io::details::context_state_inline_v<
	state_type, stack_policy, ::fast_io::details::print_context_state_storage_tag>;

/// @brief Owns raw, correctly aligned storage until a large context state has been constructed.
/// @details This first guard is required because an object's destructor is not entered when its constructor throws.
///          RAII, rather than an explicit catch block, also preserves freestanding builds compiled with exceptions
///          disabled. The typed allocator handles over-aligned state types on every supported platform.
template <typename state_type>
struct context_raw_state_guard
{
	using allocator_type = ::fast_io::native_typed_thread_local_allocator<state_type>;
	state_type *pointer{allocator_type::allocate(1u)};

	inline constexpr context_raw_state_guard() noexcept = default;
	context_raw_state_guard(context_raw_state_guard const &) = delete;
	context_raw_state_guard &operator=(context_raw_state_guard const &) = delete;

	inline constexpr state_type *release() noexcept
	{
		auto result{pointer};
		pointer = nullptr;
		return result;
	}

	inline constexpr ~context_raw_state_guard()
	{
		if (pointer != nullptr)
		{
			allocator_type::deallocate_n(pointer, 1u);
		}
	}
};

/// @brief Owns a fully constructed large incremental-protocol state.
/// @details Every normal return or exceptional exit destroys the state exactly once and then releases its raw
///          allocation. The shared `default_initializable` state contract implies a nonthrowing destructor through the
///          standard `destructible` requirement.
template <typename state_type>
struct context_constructed_state_guard
{
	using allocator_type = ::fast_io::native_typed_thread_local_allocator<state_type>;
	state_type *pointer;

	inline explicit constexpr context_constructed_state_guard(state_type *state_pointer) noexcept
		: pointer(state_pointer)
	{}
	context_constructed_state_guard(context_constructed_state_guard const &) = delete;
	context_constructed_state_guard &operator=(context_constructed_state_guard const &) = delete;

	inline constexpr ~context_constructed_state_guard()
	{
		::std::destroy_at(pointer);
		allocator_type::deallocate_n(pointer, 1u);
	}
};

/// @brief Invokes an incremental operation with a dynamically stored, value-initialized context state.
/// @details The specialization contains no automatic `state_type` object; `noinline` therefore proves that a large
///          alternative cannot enlarge the caller's hot frame. `construct_at(pointer)` preserves the same
///          value-initialization semantics as `state_type{}`, including zero-initialization only where the language's
///          value-init rules require it, and remains usable during C++20 constant evaluation when the selected allocator
///          supports constexpr dynamic allocation.
template <typename state_type, typename storage_identity, typename callback_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr decltype(auto) with_large_context_state(callback_type &&callback)
{
	static_assert(::fast_io::details::context_state_object<state_type>,
		"context state must be an unqualified, non-array, default-initializable object");
	using result_type = decltype(::std::forward<callback_type>(callback)(::std::declval<state_type &>()));
	static_assert(!::std::is_reference_v<result_type>,
		"an incremental operation must not return a reference to its temporary state");
	::fast_io::details::context_raw_state_guard<state_type> raw_guard;
	::std::construct_at(raw_guard.pointer);
	::fast_io::details::context_constructed_state_guard<state_type> state_guard{raw_guard.release()};
	if constexpr (::std::is_void_v<result_type>)
	{
		::std::forward<callback_type>(callback)(*state_guard.pointer);
	}
	else
	{
		return ::std::forward<callback_type>(callback)(*state_guard.pointer);
	}
}

/// @brief Invokes an incremental operation with bounded inline storage or isolated dynamic storage.
/// @details The callback must finish before the temporary state is destroyed and consequently may not return a state
///          reference. Small states retain allocation-free dispatch; large and over-aligned states share the same
///          value-initialization and lifetime contract through the helper above.
template <typename state_type, typename storage_identity,
		  typename stack_policy = ::fast_io::details::default_print_stack_policy, typename callback_type>
inline constexpr decltype(auto) with_context_state(callback_type &&callback)
{
	static_assert(::fast_io::details::context_state_object<state_type>,
		"context state must be an unqualified, non-array, default-initializable object");
	using result_type = decltype(::std::forward<callback_type>(callback)(::std::declval<state_type &>()));
	static_assert(!::std::is_reference_v<result_type>,
		"an incremental operation must not return a reference to its temporary state");
	// Encoding the policy type in this specialization is an ODR requirement, not cosmetic plumbing. The default policy
	// is `print_stack_policy<N>`, so separately configured builds produce different helper symbols instead of allowing
	// COMDAT merging to select an inline-vs-dynamic body compiled with another stack budget.
	if constexpr (::fast_io::details::context_state_inline_v<state_type, stack_policy, storage_identity>)
	{
		state_type state{};
		if constexpr (::std::is_void_v<result_type>)
		{
			::std::forward<callback_type>(callback)(state);
		}
		else
		{
			return ::std::forward<callback_type>(callback)(state);
		}
	}
	else
	{
		return ::fast_io::details::with_large_context_state<state_type, storage_identity>(
			::std::forward<callback_type>(callback));
	}
}

/// @brief Backward-compatible scan owner using the shared incremental-state policy.
template <typename state_type, typename callback_type,
		  typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr decltype(auto) with_scan_context_state(callback_type &&callback)
{
	return ::fast_io::details::with_context_state<state_type,
		::fast_io::details::scan_context_state_storage_tag, stack_policy>(
			::std::forward<callback_type>(callback));
}

template <typename state_type, typename callback_type>
inline constexpr decltype(auto) with_large_scan_context_state(callback_type &&callback)
{
	return ::fast_io::details::with_large_context_state<state_type,
		::fast_io::details::scan_context_state_storage_tag>(::std::forward<callback_type>(callback));
}

/// @brief Print owner using the same lifetime, alignment, constant-evaluation, and stack-budget proof as scanning.
template <typename state_type, typename callback_type,
		  typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr decltype(auto) with_print_context_state(callback_type &&callback)
{
	return ::fast_io::details::with_context_state<state_type,
		::fast_io::details::print_context_state_storage_tag, stack_policy>(
			::std::forward<callback_type>(callback));
}

} // namespace fast_io::details
