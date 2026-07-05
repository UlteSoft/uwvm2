#pragma once

namespace fast_io
{

enum class linux_statx_mask : ::std::uint_least32_t
{
	statx_type = 0x00000001,          // Request file type information (stx_mode & S_IFMT)
	statx_mode = 0x00000002,          // Request file mode/permission bits (stx_mode & ~S_IFMT)
	statx_nlink = 0x00000004,         // Request number of hard links
	statx_uid = 0x00000008,           // Request owning user ID
	statx_gid = 0x00000010,           // Request owning group ID
	statx_atime = 0x00000020,         // Request last access timestamp
	statx_mtime = 0x00000040,         // Request last modification timestamp
	statx_ctime = 0x00000080,         // Request inode status change timestamp
	statx_ino = 0x00000100,           // Request inode number
	statx_size = 0x00000200,          // Request file size in bytes
	statx_blocks = 0x00000400,        // Request number of 512-byte blocks allocated
	statx_basic_stats = 0x000007ff,   // Request all basic attributes (type through blocks)
	statx_btime = 0x00000800,         // Request file creation (birth) timestamp
	statx_mnt_id = 0x00001000,        // Request mount ID of the filesystem
	statx_dioalign = 0x00002000,      // Request direct I/O alignment constraints
	statx_mnt_id_unique = 0x00004000, // Request unique mount ID (stronger than stx_mnt_id)
	statx_subvol = 0x00008000,        // Request subvolume identifier (e.g., btrfs)
	statx_write_atomic = 0x00010000,  // Request atomic write capability information
	statx_dio_read_align = 0x00020000 // Request direct I/O read offset alignment
};

inline constexpr ::fast_io::linux_statx_mask operator&(::fast_io::linux_statx_mask x, ::fast_io::linux_statx_mask y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_mask>::type;
	return static_cast<::fast_io::linux_statx_mask>(static_cast<utype>(x) & static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_mask operator|(::fast_io::linux_statx_mask x, ::fast_io::linux_statx_mask y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_mask>::type;
	return static_cast<::fast_io::linux_statx_mask>(static_cast<utype>(x) | static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_mask operator^(::fast_io::linux_statx_mask x, ::fast_io::linux_statx_mask y) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_mask>::type;
	return static_cast<::fast_io::linux_statx_mask>(static_cast<utype>(x) ^ static_cast<utype>(y));
}

inline constexpr ::fast_io::linux_statx_mask operator~(::fast_io::linux_statx_mask x) noexcept
{
	using utype = typename ::std::underlying_type<::fast_io::linux_statx_mask>::type;
	return static_cast<::fast_io::linux_statx_mask>(~static_cast<utype>(x));
}

inline constexpr ::fast_io::linux_statx_mask &operator&=(::fast_io::linux_statx_mask &x, ::fast_io::linux_statx_mask y) noexcept
{
	return x = x & y;
}

inline constexpr ::fast_io::linux_statx_mask &operator|=(::fast_io::linux_statx_mask &x, ::fast_io::linux_statx_mask y) noexcept
{
	return x = x | y;
}

inline constexpr ::fast_io::linux_statx_mask &operator^=(::fast_io::linux_statx_mask &x, ::fast_io::linux_statx_mask y) noexcept
{
	return x = x ^ y;
}

} // namespace fast_io
