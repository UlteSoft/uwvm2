﻿#pragma once

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
        __builtin_operator_delete(ptr, sizeof(T) * n);
#else
		__builtin_trap();
#endif
	}
};

} // namespace fast_io::freestanding
