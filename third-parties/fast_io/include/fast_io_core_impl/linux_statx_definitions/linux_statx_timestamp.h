#pragma once

namespace fast_io
{

struct linux_statx_timestamp
{
	::std::int_least64_t tv_sec;   // Seconds since the Epoch (UNIX time)
	::std::uint_least32_t tv_nsec; // Nanoseconds since tv_sec
};

#if 0
template<typename T>
requires requires(T t)
{
    T::rep;
    T::period;
    t.count();
}
inline constexpr linux_statx_timestamp linux_statx_timestamp_from_duration_std(T dur) noexcept
{
    using rep = typename T::rep;
    using period = typename T::period;

    // total count
    rep c = dur.count();

    // Convert to seconds and nanoseconds WITHOUT overflow
    // tv_sec = c * period::num / period::den
    // tv_nsec = remainder * 1'000'000'000 / period::den

    // Step 1: compute whole seconds
    // We do integer division first to avoid overflow.
    constexpr std::int_least64_t num = period::num;
    constexpr std::int_least64_t den = period::den;

    // seconds = floor(c * num / den)
    // but compute as (c / den) * num + (c % den) * num / den
    std::int_least64_t sec = (static_cast<std::int_least64_t>(c) / den) * num;

    // remainder part
    std::int_least64_t rem = static_cast<std::int_least64_t>(c) % den;

    sec += (rem * num) / den;

    // Step 2: compute nanoseconds
    // nsec = ((rem * num) % den) * 1e9 / den
    std::int_least64_t rem2 = (rem * num) % den;

    std::uint_least32_t nsec =
        static_cast<std::uint_least32_t>((rem2 * 1000000000LL) / den);

    return linux_statx_timestamp{sec, nsec};
}
#endif

} // namespace fast_io
