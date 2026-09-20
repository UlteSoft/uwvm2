#pragma once

namespace fast_io
{

namespace containers
{

inline constexpr ::std::size_t npos{::std::numeric_limits<::std::size_t>::max()};

}

using ::fast_io::containers::npos;

} // namespace fast_io

namespace fast_io::containers::details
{
template <typename handle>
concept is_trivally_stored_allocator_handle = ::fast_io::freestanding::is_zero_default_constructible_v<handle> &&
											  ::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<handle> &&
											  ::std::is_trivially_copy_constructible_v<handle> &&
											  sizeof(handle) <= sizeof(handle *) && alignof(handle) <= alignof(handle *);

template <typename handle>
struct handle_holder
{
	using handle_type = handle;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	::std::conditional_t<is_trivally_stored_allocator_handle<handle>, handle, handle *> value;
	inline constexpr handle_holder() noexcept
		: value{}
	{}
	inline constexpr handle_holder(decltype(nullptr)) noexcept = delete;
	inline constexpr handle_holder(handle small_object) noexcept
		requires(is_trivally_stored_allocator_handle<handle>)
		: value(small_object)
	{}
	inline constexpr handle_holder(handle *small_object) noexcept
		requires is_trivally_stored_allocator_handle<handle>
		: value(*small_object)
	{}
	template <typename A>
	inline constexpr handle_holder(A &&large_object) noexcept
		requires(::std::same_as<::std::remove_cvref_t<A>, handle> && !is_trivally_stored_allocator_handle<handle>)
	{
		value = ::fast_io::typed_generic_allocator_adapter<handle, handle>::handle_allocate(large_object, 1);
		::std::construct_at(value, ::std::forward<handle>(large_object));
	}
	inline constexpr handle_holder(handle *large_object) noexcept
		requires(!is_trivally_stored_allocator_handle<handle>)
		: value(large_object)
	{}
	inline constexpr handle_holder(handle_holder const &) noexcept = default;
	inline constexpr handle_holder &operator=(handle_holder const &) noexcept = default;
	inline constexpr handle_holder(handle_holder &&) noexcept = default;
	inline constexpr handle_holder &operator=(handle_holder &&) noexcept = default;
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr handle const &get() const noexcept
	{
		if constexpr (is_trivally_stored_allocator_handle<handle>)
		{
			return value;
		}
		else
		{
			return *value;
		}
	}
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr handle &get() noexcept
	{
		if constexpr (is_trivally_stored_allocator_handle<handle>)
		{
			return value;
		}
		else
		{
			return *value;
		}
	}
};
template <::std::equality_comparable handle>
inline constexpr auto operator==(handle_holder<handle> left, handle_holder<handle> right) noexcept
{
	return left.get() == right.get();
}

template <::std::size_t size, bool trivial>
inline constexpr ::std::size_t cal_grow_twice_size(::std::size_t cap) noexcept
{
	constexpr ::std::size_t mx_value2{::std::numeric_limits<::std::size_t>::max() / size};
	constexpr ::std::size_t mx_value{trivial ? mx_value2 * size : mx_value2};
	constexpr ::std::size_t mx_half_value{mx_value >> 1u};
	if (cap == mx_value)
	{
		::fast_io::fast_terminate();
	}
	else if (cap > mx_half_value)
	{
		// Formal growth invariant: for every non-maximum live capacity, the
		// returned capacity is strictly larger than `cap` and no greater than
		// `mx_value`. Doubling above the half-maximum boundary would overflow;
		// saturating preserves that invariant for both byte-counted trivial
		// storage and element-counted non-trivial storage.
		return mx_value;
	}
	else if (cap == 0)
	{
		// `cap` is a byte count in the type-erased trivial path but an
		// element count in the typed non-trivial path. Therefore the same
		// one-element initial allocation is `size` bytes in the former and
		// exactly one allocator unit in the latter; returning `size` elements
		// for a non-trivial T makes initial capacity grow with sizeof(T).
		return trivial ? size : 1u;
	}
	return static_cast<::std::size_t>(cap << 1);
}

} // namespace fast_io::containers::details
