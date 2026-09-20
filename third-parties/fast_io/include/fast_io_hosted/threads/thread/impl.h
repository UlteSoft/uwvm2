#pragma once

#include <concepts>
#include <memory>
#include <type_traits>

namespace fast_io::details
{

/// @brief Formalizes the ownership and invocation boundary of a newly created thread.
/// @details Let `DF` be `decay_t<Func>` and every `DA` be the corresponding `decay_t<Arg>`. The constructor first
///          materializes one owned `DF, DA...` state from the transmitted expressions, then the new thread performs
///          exactly one `INVOKE` with those stored objects as xvalues. Requiring both constructions and
///          `invocable<DF, DA...>` makes the constraint describe that real post-decay operation instead of the
///          caller-side reference categories, while retaining `reference_wrapper` as the explicit borrowing channel.
template <typename Func, typename... Args>
concept thread_decay_invocable =
	::std::constructible_from<::std::decay_t<Func>, Func> &&
	(::std::constructible_from<::std::decay_t<Args>, Args> && ...) &&
	::std::invocable<::std::decay_t<Func>, ::std::decay_t<Args>...>;

/// @brief Owns thread-launch storage across raw allocation, object construction, and ownership transfer.
/// @details The state transition is `raw -> constructed -> released`. If decay-copy construction throws, destruction
///          observes the raw state and only deallocates storage; after `mark_constructed`, any later thread-creation
///          failure destroys the complete tuple before deallocation. A successful platform call invokes `release`,
///          transferring that identical pointer to the start routine without touching memory which the new thread may
///          already have consumed. This guard therefore closes the allocation/placement-new exception gap without
///          adding state to the launched object or its ABI.
template <typename T>
class thread_start_storage_guard
{
	T *pointer_{};
	bool constructed_{};

public:
	inline constexpr explicit thread_start_storage_guard(T *pointer) noexcept
		: pointer_{pointer}
	{}

	thread_start_storage_guard(thread_start_storage_guard const &) = delete;
	thread_start_storage_guard &operator=(thread_start_storage_guard const &) = delete;

	inline constexpr ~thread_start_storage_guard()
	{
		if (pointer_ != nullptr)
		{
			if (constructed_)
			{
				::std::destroy_at(pointer_);
			}
			using allocator_type = ::fast_io::native_typed_global_allocator<T>;
			allocator_type::deallocate_n(pointer_, 1u);
		}
	}

	inline constexpr void mark_constructed() noexcept
	{
		constructed_ = true;
	}

	[[nodiscard]] inline constexpr T *release() noexcept
	{
		T *const pointer{pointer_};
		pointer_ = nullptr;
		return pointer;
	}
};

} // namespace fast_io::details

#if (defined(_WIN32) && !defined(__WINE__)) && !defined(__CYGWIN__)
#include "win32.h"
#ifndef _WIN32_WINDOWS
#include "nt.h"
#endif
#elif defined(__MSDOS__) || defined(__DJGPP__)
#include "dos.h"
#elif defined(__wasi__)
#include "wasi.h"
#elif !defined(__SINGLE_THREAD__) && !defined(__NEWLIB__) && !defined(__MSDOS__) && __has_include(<pthread.h>)
#include "pthread.h"
#elif defined(__NEWLIB__)
#include "newlib.h"
#endif
