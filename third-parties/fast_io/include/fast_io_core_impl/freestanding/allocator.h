#pragma once

namespace fast_io::freestanding
{

template <typename T>
struct allocator
{
	inline constexpr T *allocate([[maybe_unused]] ::std::size_t n) noexcept
	{
		constexpr ::std::size_t mx{(~static_cast<::std::size_t>(0u)) / sizeof(T)};
		if (n > mx)
		{
			__builtin_trap();
		}
#if FAST_IO_HAS_BUILTIN(__builtin_operator_new)
        return static_cast<T *>(__builtin_operator_new(n * sizeof(T)));
#else
		__builtin_trap();
		return nullptr;
#endif
	}

	inline constexpr void deallocate([[maybe_unused]] T *ptr, [[maybe_unused]] ::std::size_t n) noexcept
	{
#if FAST_IO_HAS_BUILTIN(__builtin_operator_delete)
		/*
		A compiler builtin only accepts deallocation overloads declared by the
		active standard library.  __cpp_sized_deallocation permits querying the
		two-argument form, while the dependent requires-expression proves that the
		active declarations actually provide that arity. Older Clang versions
		expose the builtin but omit the sized overload in freestanding mode.
		*/
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
		if constexpr (requires(T *value, ::std::size_t bytes) {
					  __builtin_operator_delete(value, bytes);
				  })
		{
			__builtin_operator_delete(ptr, sizeof(T) * n);
			return;
		}
#endif
		(void)n;
		if constexpr (requires(T *value) { __builtin_operator_delete(value); })
		{
			__builtin_operator_delete(ptr);
		}
		else
		{
			__builtin_trap();
		}
#else
		__builtin_trap();
#endif
	}
};

} // namespace fast_io::freestanding
