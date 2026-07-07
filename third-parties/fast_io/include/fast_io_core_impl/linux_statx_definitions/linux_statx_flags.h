#pragma once

namespace fast_io
{

enum class linux_statx_flags : ::std::uint_least32_t
{
	at_empty_path = 0x1000,      // AT_EMPTY_PATH
	at_no_automount = 0x800,     // AT_NO_AUTOMOUNT
	at_symlink_nofollow = 0x100, // AT_SYMLINK_NOFOLLOW

	// statx-specific sync flags
	at_statx_sync_as_stat = 0x0000, // AT_STATX_SYNC_AS_STAT
	at_statx_force_sync = 0x2000,   // AT_STATX_FORCE_SYNC
	at_statx_dont_sync = 0x4000     // AT_STATX_DONT_SYNC
};

inline constexpr ::fast_io::linux_statx_flags operator&(::fast_io::linux_statx_flags x, ::fast_io::linux_statx_flags y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_flags>::type;
	return static_cast<::fast_io::linux_statx_flags>(static_cast<utype>(x) & static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_flags operator|(::fast_io::linux_statx_flags x, ::fast_io::linux_statx_flags y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_flags>::type;
	return static_cast<::fast_io::linux_statx_flags>(static_cast<utype>(x) | static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_flags operator^(::fast_io::linux_statx_flags x, ::fast_io::linux_statx_flags y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_flags>::type;
	return static_cast<::fast_io::linux_statx_flags>(static_cast<utype>(x) ^ static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_flags operator~(::fast_io::linux_statx_flags x) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_flags>::type;
	return static_cast<::fast_io::linux_statx_flags>(~static_cast<utype>(x));
}

inline constexpr ::fast_io::linux_statx_flags &operator&=(::fast_io::linux_statx_flags &x, ::fast_io::linux_statx_flags y) noexcept
{
	return x = x & y;
}

inline constexpr ::fast_io::linux_statx_flags &operator|=(::fast_io::linux_statx_flags &x, ::fast_io::linux_statx_flags y) noexcept
{
	return x = x | y;
}

inline constexpr ::fast_io::linux_statx_flags &operator^=(::fast_io::linux_statx_flags &x, ::fast_io::linux_statx_flags y) noexcept
{
	return x = x ^ y;
}

} // namespace fast_io
