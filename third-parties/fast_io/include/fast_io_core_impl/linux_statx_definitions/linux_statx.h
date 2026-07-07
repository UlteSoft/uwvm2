#pragma once

namespace fast_io
{

struct linux_statxbuf
{
	::std::uint_least32_t stx_mask;       /* Mask of bits indicating
							   filled fields */
	::std::uint_least32_t stx_blksize;    /* Block size for filesystem I/O */
	::std::uint_least64_t stx_attributes; /* Extra file attribute indicators */
	::std::uint_least32_t stx_nlink;      /* Number of hard links */
	::std::uint_least32_t stx_uid;        /* User ID of owner */
	::std::uint_least32_t stx_gid;        /* Group ID of owner */
	::std::uint_least16_t stx_mode;       /* File type and mode */
	::std::uint_least64_t stx_ino;        /* Inode number */
	::std::uint_least64_t stx_size;       /* Total size in bytes */
	::std::uint_least64_t stx_blocks;     /* Number of 512B blocks allocated */
	::std::uint_least64_t stx_attributes_mask;
	/* Mask to show what's supported
		in stx_attributes */

	/* The following fields are file timestamps */
	::fast_io::linux_statx_timestamp stx_atime; /* Last access */
	::fast_io::linux_statx_timestamp stx_btime; /* Creation */
	::fast_io::linux_statx_timestamp stx_ctime; /* Last status change */
	::fast_io::linux_statx_timestamp stx_mtime; /* Last modification */

	/* If this file represents a device, then the next two
		fields contain the ID of the device */
	::std::uint_least32_t stx_rdev_major; /* Major ID */
	::std::uint_least32_t stx_rdev_minor; /* Minor ID */

	/* The next two fields contain the ID of the device
		containing the filesystem where the file resides */
	::std::uint_least32_t stx_dev_major; /* Major ID */
	::std::uint_least32_t stx_dev_minor; /* Minor ID */

	::std::uint_least64_t stx_mnt_id; /* Mount ID */

	/* Direct I/O alignment restrictions */
	::std::uint_least32_t stx_dio_mem_align;
	::std::uint_least32_t stx_dio_offset_align;

	::std::uint_least64_t stx_subvol; /* Subvolume identifier */

	/* Direct I/O atomic write limits */
	::std::uint_least32_t stx_atomic_write_unit_min;
	::std::uint_least32_t stx_atomic_write_unit_max;
	::std::uint_least32_t stx_atomic_write_segments_max;

	/* File offset alignment for direct I/O reads */
	::std::uint_least32_t stx_dio_read_offset_align;

	/* Direct I/O atomic write max opt limit */
	::std::uint_least32_t stx_atomic_write_unit_max_opt;
};

} // namespace fast_io
