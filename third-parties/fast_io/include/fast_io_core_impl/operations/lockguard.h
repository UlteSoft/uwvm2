#pragma once

/*
 * RAII guards for normalized stream mutex protocols.
 *
 * Operation engines instantiate these guards only after `refs/mutex.h` has
 * proved a complete mutex/unlocked-reference pair. The guard owns or borrows
 * the projected mutex proxy, acquires it for one logical operation, and
 * releases it on scope exit. It does not decide whether synchronization is
 * needed or how a stream is normalized.
 */

namespace fast_io::operations::decay
{

template <typename mtx_type>
struct stream_ref_decay_lock_guard
{
	using mutex_type = mtx_type;
	mutex_type device;
	/// @brief Acquires a mutex proxy after transferring the by-value parameter into stable guard storage.
	/// @details Class-template argument deduction intentionally normalizes a value or reference CPO result to one proxy
	///          value. Constructing the member from the parameter as an rvalue removes the accidental second copy: a
	///          copyable stable lvalue result still copies into the parameter, while a move-only prvalue result moves
	///          once into storage. The complete mutex concepts prove both construction steps before selecting this guard.
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline explicit constexpr stream_ref_decay_lock_guard(mutex_type d)
		: device(static_cast<mutex_type &&>(d))
	{
		device.lock();
	}
	inline stream_ref_decay_lock_guard(stream_ref_decay_lock_guard const &) = delete;
	inline stream_ref_decay_lock_guard &operator=(stream_ref_decay_lock_guard const &) = delete;
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr ~stream_ref_decay_lock_guard()
	{
		device.unlock();
	}
};

template <typename mtx_type>
struct unlock_stream_ref_decay_lock_guard
{
	using mutex_type = mtx_type;
	mutex_type &device;
	/// @brief Temporarily releases the mutex owned by an existing lock guard.
	/// @details Referencing the outer guard's storage is essential for move-only proxies and also preserves proxy
	///          identity: copying a handle-like proxy could unlock a different logical object. The outer guard is
	///          immovable and must outlive this nested guard, so the reference remains stable until relocking completes.
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
	inline explicit constexpr unlock_stream_ref_decay_lock_guard(stream_ref_decay_lock_guard<mtx_type> &lg)
		: device(lg.device)
	{
		device.unlock();
	}
	inline unlock_stream_ref_decay_lock_guard(unlock_stream_ref_decay_lock_guard const &) = delete;
	inline unlock_stream_ref_decay_lock_guard &operator=(unlock_stream_ref_decay_lock_guard const &) = delete;
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr ~unlock_stream_ref_decay_lock_guard()
	{
		device.lock();
	}
};

} // namespace fast_io::operations::decay
