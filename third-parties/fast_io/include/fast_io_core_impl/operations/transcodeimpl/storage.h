#pragma once

/**
 * @file
 * @brief Defines underlying-handle storage policies for transcoder adapters.
 *
 * Lvalues are borrowed by address and rvalues are owned by value. The policy
 * is selected once by the factory, so adapter code can uniformly call get()
 * without accidentally copying a stream observer or extending a dangling
 * reference.
 */

namespace fast_io::details
{

/** @brief Stores a non-owning pointer when a factory receives an lvalue handle. */
template <typename T>
struct borrowed_otranscoder_handle
{
	using handle_type = T;

	handle_type *ptr{};

	/** @brief Returns the borrowed handle without changing its ownership. */
	inline constexpr handle_type &get() const noexcept
	{
		return *ptr;
	}
};

/** @brief Owns a moved or copied handle when a factory receives an rvalue. */
template <typename T>
struct owned_otranscoder_handle
{
	using handle_type = T;

	handle_type value;

	template <typename U>
		requires ::std::constructible_from<handle_type, U>
	/** @brief Constructs owned storage from the forwarded handle expression. */
	inline explicit constexpr owned_otranscoder_handle(U &&handle)
		: value(::std::forward<U>(handle))
	{}

	/** @brief Returns the owned mutable handle used by adapter operations. */
	inline constexpr handle_type &get() noexcept
	{
		return value;
	}
};

/** @brief Selects borrowed storage for lvalues and owned storage for rvalues. */
template <typename T>
using otranscoder_handle_storage_t = ::std::conditional_t<
	::std::is_lvalue_reference_v<T>,
	::fast_io::details::borrowed_otranscoder_handle<::std::remove_reference_t<T>>,
	::fast_io::details::owned_otranscoder_handle<::std::remove_cvref_t<T>>>;

/** @brief Materializes the output-handle storage selected from value category. */
template <typename T>
inline constexpr auto make_otranscoder_handle_storage(T &&handle)
{
	using storage_type = ::fast_io::details::otranscoder_handle_storage_t<T &&>;
	if constexpr (::std::is_lvalue_reference_v<T &&>)
	{
		// Borrow lvalues so the adapter never copies an existing stream owner.
		return storage_type{__builtin_addressof(handle)};
	}
	else
	{
		// Own rvalues so the adapter cannot retain a dangling handle reference.
		return storage_type{::std::forward<T>(handle)};
	}
}

/** @brief Reuses the same lifetime policy for input-only adapters. */
template <typename T>
using itranscoder_handle_storage_t =
	::fast_io::details::otranscoder_handle_storage_t<T>;

/** @brief Materializes lifetime-safe storage for an input handle. */
template <typename T>
inline constexpr auto make_itranscoder_handle_storage(T &&handle)
{
	return ::fast_io::details::make_otranscoder_handle_storage(
		::std::forward<T>(handle));
}

/** @brief Shares one duplex handle between independently stateful child adapters. */
template <typename handle_storage>
struct iotranscoder_shared_handle
{
	// Both directional adapters borrow the one storage object owned by their
	// basic_iotranscoder parent. The callback is intentionally type-erased to
	// keep this storage independent of the two engine types.
	handle_storage *ptr{};
	void *tie_ptr{};
	void (*tie_flush)(void *){};

	/** @brief Returns the common underlying handle for either direction. */
	inline constexpr decltype(auto) get() const
		noexcept(noexcept(ptr->get()))
	{
		return ptr->get();
	}

	/** @brief Establishes output visibility before an input refill when tied. */
	inline void before_input_refill() const
	{
		// A tied input operation establishes visibility only. Terminal output
		// finish would close a message and make subsequent writes invalid.
		if (tie_flush != nullptr)
		{
			// Invoke the parent's type-erased sync-flush callback when installed.
			tie_flush(tie_ptr);
		}
	}
};

} // namespace fast_io::details
