#pragma once

#include <errno.h>
#include <pthread.h>

namespace fast_io
{

struct posix_pthread_mutex
{
	using native_handle_type = pthread_mutex_t;
	native_handle_type mutex;
	inline explicit posix_pthread_mutex() noexcept
		: mutex(PTHREAD_MUTEX_INITIALIZER)
	{}
	inline posix_pthread_mutex(posix_pthread_mutex const &) = delete;
	inline posix_pthread_mutex &operator=(posix_pthread_mutex const &) = delete;
	inline void lock()
	{
#if (defined(__APPLE__) || defined(__DARWIN_C_LEVEL)) && FAST_IO_HAS_BUILTIN(__builtin_available)
		if (__builtin_available(macOS 10.4, iOS 2.0, *)) [[likely]]
		{
			auto const res{::fast_io::noexcept_call(::pthread_mutex_lock, __builtin_addressof(mutex))};
			if (res != 0) [[unlikely]]
			{
				::fast_io::throw_posix_error(res);
			}
			return;
		}
		::fast_io::throw_posix_error(ENOSYS);
#else
		auto const res{::fast_io::noexcept_call(::pthread_mutex_lock, __builtin_addressof(mutex))};
		if (res != 0) [[unlikely]]
		{
			::fast_io::throw_posix_error(res);
		}
#endif
	}
	inline bool try_lock() noexcept
	{
#if (defined(__APPLE__) || defined(__DARWIN_C_LEVEL)) && FAST_IO_HAS_BUILTIN(__builtin_available)
		if (__builtin_available(macOS 10.4, iOS 2.0, *)) [[likely]]
		{
			return !noexcept_call(pthread_mutex_trylock, __builtin_addressof(mutex));
		}
		return false;
#else
		return !noexcept_call(pthread_mutex_trylock, __builtin_addressof(mutex));
#endif
	}
	inline void unlock() noexcept
	{
#if (defined(__APPLE__) || defined(__DARWIN_C_LEVEL)) && FAST_IO_HAS_BUILTIN(__builtin_available)
		if (__builtin_available(macOS 10.4, iOS 2.0, *)) [[likely]]
		{
			noexcept_call(pthread_mutex_unlock, __builtin_addressof(mutex));
		}
#else
		noexcept_call(pthread_mutex_unlock, __builtin_addressof(mutex));
#endif
	}

	inline ~posix_pthread_mutex()
	{
#if (defined(__APPLE__) || defined(__DARWIN_C_LEVEL)) && FAST_IO_HAS_BUILTIN(__builtin_available)
		if (__builtin_available(macOS 10.4, iOS 2.0, *)) [[likely]]
		{
			noexcept_call(pthread_mutex_destroy, __builtin_addressof(mutex));
		}
#else
		noexcept_call(pthread_mutex_destroy, __builtin_addressof(mutex));
#endif
	}
};

} // namespace fast_io
