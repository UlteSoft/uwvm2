#pragma once

// Error-returning POSIX filesystem operations. These share one implementation
// with and without C++ exceptions; no failure calls throw_posix_error().
// Include through fast_io.h / fast_io_hosted.h, after the POSIX file provider.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
namespace fast_io
{
struct posix_status_result
{
	posix_file_status value{};
	int error{};
	inline explicit constexpr operator bool() const noexcept { return error == 0; }
};
struct posix_operation_result
{
	int error{};
	inline explicit constexpr operator bool() const noexcept { return error == 0; }
};
struct posix_read_result
{
	::std::size_t transferred{};
	int error{};
	inline explicit constexpr operator bool() const noexcept { return error == 0; }
};
// Read and write share one byte-transfer result layout.
using posix_write_result = posix_read_result;
struct posix_getfl_result
{
	int flags{};
	int error{};
	inline explicit constexpr operator bool() const noexcept { return error == 0; }
};
struct posix_seek_result
{
	::fast_io::intfpos_t position{};
	int error{};
	inline explicit constexpr operator bool() const noexcept { return error == 0; }
};
struct posix_openat_result
{
	posix_file file{};
	int error{};
	inline posix_openat_result(posix_file&& owner, int native_error) noexcept
		: file(static_cast<posix_file&&>(owner)), error(native_error) {}
	posix_openat_result(posix_openat_result const&) = delete;
	posix_openat_result& operator=(posix_openat_result const&) = delete;
	posix_openat_result(posix_openat_result&&) noexcept = default;
	posix_openat_result& operator=(posix_openat_result&&) noexcept = default;
	inline explicit constexpr operator bool() const noexcept { return error == 0 && file.native_handle() != -1; }
};
namespace details::posix_nothrow_abi
{
// Preserve fast_io intfpos_t large-file positions even when native off_t is
// 32-bit. These are the real SDK libc ABI types, not guessed kernel words.
#if defined(__GLIBC__)
using seek_offset_type = ::__off64_t;
#elif defined(__BIONIC__)
using seek_offset_type = ::off64_t;
#else
using seek_offset_type = ::off_t;
#endif
// Match the selected libc's *native struct stat* and off_t declarations, not
// a guessed kernel stat structure or the presence of optional stat64 names.
#if defined(__APPLE__) && defined(__MACH__)
extern int fd_status(int, struct ::stat*) noexcept __DARWIN_INODE64(fstat);
extern int path_status(char const*, struct ::stat*) noexcept __DARWIN_INODE64(stat);
extern int link_status(char const*, struct ::stat*) noexcept __DARWIN_INODE64(lstat);
[[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
extern int at_status(int, char const*, struct ::stat*, int) noexcept __DARWIN_INODE64(fstatat);
[[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
extern int at_open(int, char const*, int, ...) noexcept __DARWIN_NOCANCEL(openat);
extern int path_open(char const*, int, ...) noexcept __DARWIN_ALIAS_C(open);
extern int fd_truncate(int, ::off_t) noexcept __asm__("_ftruncate");
extern ::ssize_t fd_read(int, void*, ::std::size_t) noexcept __DARWIN_ALIAS_C(read);
extern ::ssize_t fd_write(int, void const*, ::std::size_t) noexcept __DARWIN_ALIAS_C(write);
extern int fd_close(int) noexcept __DARWIN_ALIAS_C(close);
extern int fd_sync(int) noexcept __DARWIN_ALIAS_C(fsync);
#elif defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
# if defined(__USE_TIME64_REDIRECTS) || (defined(__USE_TIME_BITS64) && defined(__TIMESIZE) && __TIMESIZE == 32)
extern int fd_status(int, struct ::stat*) noexcept __asm__("__fstat64_time64");
extern int at_status(int, char const*, struct ::stat*, int) noexcept __asm__("__fstatat64_time64");
# else
extern int fd_status(int, struct ::stat*) noexcept __asm__("fstat64");
extern int at_status(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat64");
# endif
extern int at_open(int, char const*, int, ...) noexcept __asm__("openat64");
extern int fd_truncate(int, ::off_t) noexcept __asm__("ftruncate64");
extern ::ssize_t fd_read(int, void*, ::std::size_t) noexcept __asm__("read");
extern ::ssize_t fd_write(int, void const*, ::std::size_t) noexcept __asm__("write");
extern int fd_close(int) noexcept __asm__("close");
extern int fd_sync(int) noexcept __asm__("fsync");
#else
// musl's 32-bit time64 redirection changes stat, but not off_t/openat.
# if defined(_REDIR_TIME64) && _REDIR_TIME64
extern int fd_status(int, struct ::stat*) noexcept __asm__("__fstat_time64");
extern int at_status(int, char const*, struct ::stat*, int) noexcept __asm__("__fstatat_time64");
# else
extern int fd_status(int, struct ::stat*) noexcept __asm__("fstat");
extern int at_status(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat");
# endif
extern int at_open(int, char const*, int, ...) noexcept __asm__("openat");
# if defined(__BIONIC__) && defined(__USE_FILE_OFFSET64)
extern int fd_truncate(int, ::off_t) noexcept __asm__("ftruncate64");
# else
extern int fd_truncate(int, ::off_t) noexcept __asm__("ftruncate");
# endif
extern ::ssize_t fd_read(int, void*, ::std::size_t) noexcept __asm__("read");
extern ::ssize_t fd_write(int, void const*, ::std::size_t) noexcept __asm__("write");
extern int fd_close(int) noexcept __asm__("close");
extern int fd_sync(int) noexcept __asm__("fsync");
#endif
#if defined(__APPLE__) && defined(__MACH__)
extern seek_offset_type fd_seek(int, seek_offset_type, int) noexcept __asm__("_lseek");
#elif defined(__GLIBC__) || defined(__BIONIC__)
extern seek_offset_type fd_seek(int, seek_offset_type, int) noexcept __asm__("lseek64");
#else
extern seek_offset_type fd_seek(int, seek_offset_type, int) noexcept __asm__("lseek");
#endif
#if defined(__linux__)
// Linux's public fdatasync SDK ABI is int(int). It has no off_t/stat/time64
// payload or symbol redirection. Only use this noexcept libc alias when the
// target headers do not supply the real raw syscall number.
extern int fd_data_sync(int) noexcept __asm__("fdatasync");
#endif
#if defined(__linux__)
// posix_fadvise returns zero or the POSIX error value itself; it is NOT a
// -1/errno API. Use the actual SDK large-file libc ABI, which owns target
// register-pair ordering/alignment and s390 packed syscall arguments.
# if defined(__GLIBC__) || defined(__BIONIC__)
extern int fd_advise(int, seek_offset_type, seek_offset_type, int) noexcept __asm__("posix_fadvise64");
# else
extern int fd_advise(int, seek_offset_type, seek_offset_type, int) noexcept __asm__("posix_fadvise");
# endif
// Only source-confirmed SDK symbol contracts are selected here. __GLIBC__
// also appears in uClibc; its optional large-file symbols need their own SDK
// binding and must not inherit GNU glibc qualification. Other Linux libcs,
// including musl, need actual SDK/target binding before enabling this API.
// glibc 2.3.x macros cannot prove the 2.3.3 wide-length default ABI; automatic
// selection conservatively starts at 2.4. Older SDKs retain their provider.
// Android introduced the wide public entry in API 21. Keep the template
// name present everywhere for dependent public requires-expressions, but
// do not instantiate an unqualified or unavailable runtime symbol.
# if defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
	(__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
inline constexpr bool fd_advise_sdk_available = true;
# elif defined(__BIONIC__) && defined(__ANDROID_API__) && __ANDROID_API__ >= 21
inline constexpr bool fd_advise_sdk_available = true;
# else
inline constexpr bool fd_advise_sdk_available = false;
# endif
#endif
// The readonly getter below accepts F_GETFL only. No arbitrary fcntl
// command, locking-structure ABI or pointer payload is accepted.
#if defined(__APPLE__) && defined(__MACH__)
extern int fd_get_flags(int, int, ...) noexcept __DARWIN_ALIAS_C(fcntl);
#elif defined(__GLIBC__) && defined(__USE_TIME64_REDIRECTS)
extern int fd_get_flags(int, int, ...) noexcept __asm__("__fcntl_time64");
#elif defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
extern int fd_get_flags(int, int, ...) noexcept __asm__("fcntl64");
#else
extern int fd_get_flags(int, int, ...) noexcept __asm__("fcntl");
#endif
// F_SETFL has one int scalar payload. Keep its native aliases separate from
// the readonly getter; neither public API accepts an arbitrary fcntl command
// or a pointer/file-lock structure. SDK redirects select the real libc ABI.
#if defined(__APPLE__) && defined(__MACH__)
extern int fd_set_flags(int, int, ...) noexcept __DARWIN_ALIAS_C(fcntl);
#elif defined(__GLIBC__) && defined(__USE_TIME64_REDIRECTS)
extern int fd_set_flags(int, int, ...) noexcept __asm__("__fcntl_time64");
#elif defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
extern int fd_set_flags(int, int, ...) noexcept __asm__("fcntl64");
#else
extern int fd_set_flags(int, int, ...) noexcept __asm__("fcntl");
#endif
#if defined(__APPLE__) && defined(__MACH__) && defined(F_RDAHEAD) && defined(F_RDADVISE)
// These two SDK commands use distinct scalar and owned-radvisory payloads.
// The public methods below expose neither arbitrary fcntl commands nor pointers.
// Keep the exact target SDK cancellation/UNIX conformance symbol suffixes.
extern int fd_read_advice(int, int, ...) noexcept __DARWIN_ALIAS_C(fcntl);
#endif
inline int last_error() noexcept { int const error{errno}; return error == 0 ? EIO : error; }
inline int checked_at_status(int directory, char const* path, struct ::stat* output, int flags) noexcept
{
	// path borrows a complete caller-owned NUL-terminated pathname, and output
	// borrows one complete native struct stat only through this synchronous call.
#if defined(__APPLE__) && defined(__MACH__) && FAST_IO_HAS_BUILTIN(__builtin_available)
	if (__builtin_available(macOS 10.10, iOS 8.0, *)) [[likely]]
		return at_status(directory, path, output, flags);
	if (directory == AT_FDCWD)
	{
		if (flags == 0) return path_status(path, output);
# if defined(AT_SYMLINK_NOFOLLOW)
		if (flags == AT_SYMLINK_NOFOLLOW) return link_status(path, output);
# endif
	}
	errno = ENOSYS;
	return -1;
#else
	return at_status(directory, path, output, flags);
#endif
}
inline int checked_at_open(int directory, char const* path, int flags, ::mode_t permissions) noexcept
{
	// The kernel/libc consumes the pathname during this call; it is never kept.
#if defined(__APPLE__) && defined(__MACH__) && FAST_IO_HAS_BUILTIN(__builtin_available)
	if (__builtin_available(macOS 10.10, iOS 8.0, *)) [[likely]]
		return at_open(directory, path, flags, permissions);
	if (directory == AT_FDCWD) return path_open(path, flags, permissions);
	errno = ENOSYS;
	return -1;
#else
	return at_open(directory, path, flags, permissions);
#endif
}
} // namespace details::posix_nothrow_abi

template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_status_result posix_status_nothrow(basic_posix_family_io_observer<family, char_type> file) noexcept
{
	struct ::stat native{};
	// native is one complete local cell; the borrowed observer never owns or
	// changes the descriptor, and no native structure escapes this function.
	if (details::posix_nothrow_abi::fd_status(file.native_handle(), __builtin_addressof(native)) != 0)
		return {{}, details::posix_nothrow_abi::last_error()};
	return {details::struct_stat_to_posix_file_status(native), 0};
}
inline posix_status_result posix_fstatat_nothrow(posix_at_entry directory, char const* path, int flags = 0) noexcept
{
	struct ::stat native{};
	if (details::posix_nothrow_abi::checked_at_status(directory.fd, path, __builtin_addressof(native), flags) != 0)
		return {{}, details::posix_nothrow_abi::last_error()};
	return {details::struct_stat_to_posix_file_status(native), 0};
}
inline posix_openat_result posix_openat_nothrow(posix_at_entry directory, char const* path, int native_flags,
	perms permissions = static_cast<perms>(436)) noexcept
{
	// native_flags are passed exactly. Unlike open_mode, this API never adds
	// O_CREAT/O_TRUNC; the caller controls destructive operations and CLOEXEC.
	int const descriptor{details::posix_nothrow_abi::checked_at_open(directory.fd, path, native_flags,
		static_cast<::mode_t>(permissions))};
	if (descriptor < 0) return {posix_file{}, details::posix_nothrow_abi::last_error()};
	// Exclusive ownership transfers directly into this move-only result. The
	// pathname borrow ends here; only the owned file survives the call.
	return {posix_file{descriptor}, 0};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_truncate_nothrow(basic_posix_family_io_observer<family, char_type> file,
	::fast_io::uintfpos_t size) noexcept
{
	if (size > static_cast<::fast_io::uintfpos_t>(::std::numeric_limits<::off_t>::max())) return {EINVAL};
	if (details::posix_nothrow_abi::fd_truncate(file.native_handle(), static_cast<::off_t>(size)) != 0)
		return {details::posix_nothrow_abi::last_error()};
	return {};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_getfl_result posix_getfl_nothrow(basic_posix_family_io_observer<family, char_type> file) noexcept
{
	// Only the descriptor number is borrowed during ONE readonly F_GETFL.
	// No owner is released, flag changed, pointer used or errno retry performed.
#if defined(__linux__) && defined(__NR_fcntl)
	auto const flags{::fast_io::system_call<__NR_fcntl, int>(file.native_handle(), F_GETFL, 0)};
	if (::fast_io::linux_system_call_fails(flags)) return {0, static_cast<int>(-flags)};
#elif defined(__linux__) && defined(__NR_fcntl64)
	// F_GETFL has the same scalar ABI on the target's fcntl64 syscall. This
	// does not expose its different file-lock structure or invent an NR alias.
	auto const flags{::fast_io::system_call<__NR_fcntl64, int>(file.native_handle(), F_GETFL, 0)};
	if (::fast_io::linux_system_call_fails(flags)) return {0, static_cast<int>(-flags)};
#else
	auto const flags{details::posix_nothrow_abi::fd_get_flags(file.native_handle(), F_GETFL, 0)};
	if (flags < 0) return {0, details::posix_nothrow_abi::last_error()};
#endif
	return {flags, 0};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_setfl_nothrow(basic_posix_family_io_observer<family, char_type> file,
	int native_flags) noexcept
{
	// Borrow only this live descriptor number for ONE F_SETFL with the exact
	// caller-supplied int flags. No pointer, owner release, allocation, retry,
	// implicit get/verify/rollback or exception enters this operation.
	// Status flags belong to the shared open file description. The caller must
	// serialize any read/modify/verify/rollback transaction and pin FD lifetime.
	// Native F_SETFL success can ignore unsupported flags (for example Linux
	// O_SYNC/O_DSYNC); success here does not promise requested bits took effect.
#if defined(__linux__) && defined(__NR_fcntl)
	auto const result{::fast_io::system_call<__NR_fcntl, int>(file.native_handle(), F_SETFL, native_flags)};
	if (::fast_io::linux_system_call_fails(result)) return {static_cast<int>(-result)};
#elif defined(__linux__) && defined(__NR_fcntl64)
	// The target SDK advertises this real syscall. F_SETFL retains its int
	// scalar ABI; no different fcntl64 file-lock structure is exposed.
	auto const result{::fast_io::system_call<__NR_fcntl64, int>(file.native_handle(), F_SETFL, native_flags)};
	if (::fast_io::linux_system_call_fails(result)) return {static_cast<int>(-result)};
#else
	auto const result{details::posix_nothrow_abi::fd_set_flags(file.native_handle(), F_SETFL, native_flags)};
	if (result < 0) return {details::posix_nothrow_abi::last_error()};
#endif
	return {};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_seek_result posix_seek_nothrow(basic_posix_family_io_observer<family, char_type> file,
	::fast_io::intfpos_t offset, ::fast_io::seekdir direction) noexcept
{
	using native_offset_type = details::posix_nothrow_abi::seek_offset_type;
	int native_direction{};
	switch (direction)
	{
	case ::fast_io::seekdir::beg: native_direction = SEEK_SET; break;
	case ::fast_io::seekdir::cur: native_direction = SEEK_CUR; break;
	case ::fast_io::seekdir::end: native_direction = SEEK_END; break;
	default: return {0, EINVAL};
	}
#if defined(__linux__) && (defined(__NR_llseek) || defined(__NR__llseek))
	if constexpr (::std::numeric_limits<native_offset_type>::digits < ::std::numeric_limits<::std::int64_t>::digits)
	{
		// Preserve the existing fast_io Linux32 large-file contract even for a
		// libc with native off_t32 and no qualified public lseek64 declaration.
		// __NR_llseek / __NR__llseek come from target kernel syscall headers; the real
		// five-argument ABI is fd, high32, low32, loff_t* result, whence. RV32
		// need not advertise the different single-register __NR_lseek ABI.
		if constexpr (::std::numeric_limits<::fast_io::intfpos_t>::digits > ::std::numeric_limits<::std::int64_t>::digits)
		{
			if (offset < static_cast<::fast_io::intfpos_t>((::std::numeric_limits<::std::int64_t>::min)()) ||
				offset > static_cast<::fast_io::intfpos_t>((::std::numeric_limits<::std::int64_t>::max)())) return {0, EOVERFLOW};
		}
		::std::int64_t position{};
		::std::uint64_t const offset_bits{static_cast<::std::uint64_t>(offset)};
		// position is one complete owned 8-byte loff_t-compatible cell. The
		// kernel borrows/writes it only during this call; no cursor is advanced,
		// pointer escapes, ownership changes or libc errno access occurs.
# if defined(__NR_llseek) && defined(__NR__llseek)
		static_assert(__NR_llseek == __NR__llseek, "Conflicting target llseek syscall declarations");
# endif
# if defined(__NR_llseek)
		constexpr auto syscall_number{__NR_llseek};
# else
		// i386/ARM headers use _llseek; generic/RV32 headers use llseek.
		// Select the actual target constant, never define a guessed NR alias.
		constexpr auto syscall_number{__NR__llseek};
# endif
		auto const result{::fast_io::system_call<syscall_number, int>(file.native_handle(),
			static_cast<unsigned long>(offset_bits >> 32u),
			static_cast<unsigned long>(static_cast<::std::uint32_t>(offset_bits)),
			__builtin_addressof(position), static_cast<unsigned int>(native_direction))};
		if (::fast_io::linux_system_call_fails(result)) return {0, static_cast<int>(-result)};
		if (result != 0 || position < 0) return {0, EIO};
		if constexpr (::std::numeric_limits<::fast_io::intfpos_t>::digits < ::std::numeric_limits<::std::int64_t>::digits)
		{
			if (position > static_cast<::std::int64_t>((::std::numeric_limits<::fast_io::intfpos_t>::max)())) return {0, EOVERFLOW};
		}
		return {static_cast<::fast_io::intfpos_t>(position), 0};
	}
#endif
	if constexpr (::std::numeric_limits<native_offset_type>::digits < ::std::numeric_limits<::fast_io::intfpos_t>::digits)
	{
		if (offset < static_cast<::fast_io::intfpos_t>((::std::numeric_limits<native_offset_type>::min)()) ||
			offset > static_cast<::fast_io::intfpos_t>((::std::numeric_limits<native_offset_type>::max)())) return {0, EOVERFLOW};
	}
	// This synchronous observer borrow changes only the kernel file position;
	// no ownership, user pointer, retry or throwing adapter is involved. The
	// SDK-selected wide offset type and libc symbol handle Linux32's split-offset
	// kernel ABI, including RV32 without __NR_lseek, rather than guessing it.
	auto const position{details::posix_nothrow_abi::fd_seek(file.native_handle(), static_cast<native_offset_type>(offset), native_direction)};
	if (position == static_cast<native_offset_type>(-1)) return {0, details::posix_nothrow_abi::last_error()};
	if constexpr (::std::numeric_limits<native_offset_type>::digits > ::std::numeric_limits<::fast_io::intfpos_t>::digits)
	{
		if (position < static_cast<native_offset_type>((::std::numeric_limits<::fast_io::intfpos_t>::min)()) ||
			position > static_cast<native_offset_type>((::std::numeric_limits<::fast_io::intfpos_t>::max)())) return {0, EOVERFLOW};
	}
	return {static_cast<::fast_io::intfpos_t>(position), 0};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_read_result posix_read_nothrow(basic_posix_family_io_observer<family, char_type> file,
	void* buffer, ::std::size_t count) noexcept
{
	// buffer borrows [buffer,buffer+count) of caller-owned writable bytes for
	// this synchronous call. No pointer is advanced or retained. Successful
	// transferred==0 is EOF; EINTR and other native errors remain caller policy.
	if (count > static_cast<::std::size_t>(::std::numeric_limits<::ssize_t>::max())) return {0, EINVAL};
	auto const transferred{details::posix_nothrow_abi::fd_read(file.native_handle(), buffer, count)};
	if (transferred < 0) return {0, details::posix_nothrow_abi::last_error()};
	if (static_cast<::std::size_t>(transferred) > count) return {0, EIO};
	return {static_cast<::std::size_t>(transferred), 0};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_write_result posix_write_nothrow(basic_posix_family_io_observer<family, char_type> file,
	void const* buffer, ::std::size_t count) noexcept
{
	// buffer borrows [buffer,buffer+count) of caller-owned readable bytes only
	// through this one synchronous native write. No pointer is advanced/retained,
	// allocation, locking, flushing or retry occurs. Partial writes and EINTR
	// remain caller policy; the observer never owns or closes the descriptor.
	if (count > static_cast<::std::size_t>(::std::numeric_limits<::ssize_t>::max())) return {0, EINVAL};
#if defined(__linux__) && defined(__NR_write)
	// Use fast_io's existing Linux syscall primitive. Native errors remain
	// negative errno; the caller-visible interrupted errno is unchanged.
	auto const transferred{::fast_io::system_call<__NR_write, ::std::ptrdiff_t>(file.native_handle(), buffer, count)};
	if (::fast_io::linux_system_call_fails(transferred)) return {0, static_cast<int>(-transferred)};
#else
	auto const transferred{details::posix_nothrow_abi::fd_write(file.native_handle(), buffer, count)};
	if (transferred < 0) return {0, details::posix_nothrow_abi::last_error()};
#endif
	if (static_cast<::std::size_t>(transferred) > count) return {0, EIO};
	return {static_cast<::std::size_t>(transferred), 0};
}
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_fsync_nothrow(basic_posix_family_io_observer<family, char_type> file) noexcept
{
	// The borrowed observer neither owns nor closes the descriptor. Flush any
	// user-space buffer first; this issues exactly one native fsync request with
	// no retry. It does not substitute Darwin F_FULLFSYNC or prove power-loss
	// durability/parent-directory publication for an unqualified filesystem.
#if defined(__linux__) && defined(__NR_fsync)
	auto const result{::fast_io::system_call<__NR_fsync, int>(file.native_handle())};
	if (::fast_io::linux_system_call_fails(result)) return {static_cast<int>(-result)};
	if (result != 0) return {EIO};
#else
	if (details::posix_nothrow_abi::fd_sync(file.native_handle()) != 0)
		return {details::posix_nothrow_abi::last_error()};
#endif
	return {};
}
#if defined(__linux__)
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_fdatasync_nothrow(basic_posix_family_io_observer<family, char_type> file) noexcept
{
	// Borrow only the caller-pinned descriptor number through ONE native
	// fdatasync. Flush user-space buffers first. This operation neither closes
	// the observer, allocates, throws, retries EINTR nor substitutes fsync.
	// Preserve every native error, including ENOSYS, for explicit caller policy.
	// A successful request is not a power-loss or parent-directory publication
	// qualification; it does not flush metadata unrelated to data retrieval.
# if defined(__NR_fdatasync)
	auto const result{::fast_io::system_call<__NR_fdatasync, int>(file.native_handle())};
	if (::fast_io::linux_system_call_fails(result)) return {static_cast<int>(-result)};
# else
	auto const result{details::posix_nothrow_abi::fd_data_sync(file.native_handle())};
	if (result < 0) return {details::posix_nothrow_abi::last_error()};
# endif
	return {};
}
#endif
#if defined(__linux__)
template <::fast_io::posix_family family, ::std::integral char_type>
requires (details::posix_nothrow_abi::fd_advise_sdk_available &&
	::std::numeric_limits<details::posix_nothrow_abi::seek_offset_type>::is_signed &&
	::std::numeric_limits<details::posix_nothrow_abi::seek_offset_type>::digits >=
		::std::numeric_limits<::fast_io::intfpos_t>::digits)
inline posix_operation_result posix_fadvise_nothrow(basic_posix_family_io_observer<family, char_type> file,
	::fast_io::intfpos_t offset, ::fast_io::intfpos_t length, int native_advice) noexcept
{
	// Borrow only the caller-pinned descriptor number through ONE real SDK
	// posix_fadvise call. No pointer, ownership transfer, allocation, retry,
	// errno access or software emulation is introduced. The SDK large-file
	// ABI carries both signed offsets without narrowing, including on i386.
	// Negative values and invalid advice are forwarded unchanged: only the
	// actual native implementation determines its descriptor/error precedence.
	// No hint is silently clamped, validated, ignored or upgraded here. In a
	// unqualified/narrow/older SDK this overload is not callable; dependent
	// consumers retain their previous provider instead of receiving a guessed
	// EOVERFLOW or referencing an unavailable large-file symbol.
	using native_offset_type = details::posix_nothrow_abi::seek_offset_type;
	return {details::posix_nothrow_abi::fd_advise(file.native_handle(),
		static_cast<native_offset_type>(offset), static_cast<native_offset_type>(length), native_advice)};
}
#endif
#if defined(__APPLE__) && defined(__MACH__) && defined(F_RDAHEAD) && defined(F_RDADVISE)
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_readahead_nothrow(basic_posix_family_io_observer<family, char_type> file,
	bool enabled) noexcept
{
	// ONE SDK F_RDAHEAD borrows only the caller-pinned descriptor number and
	// an int 0/1 switch. The observer never owns, closes or duplicates the FD.
	// This modifies the shared open file description's native read-ahead flag;
	// no allocation, retry, verification, software emulation or cache guarantee.
	auto const result{details::posix_nothrow_abi::fd_read_advice(file.native_handle(), F_RDAHEAD, static_cast<int>(enabled))};
	if (result < 0) return {details::posix_nothrow_abi::last_error()};
	return {};
}
template <::fast_io::posix_family family, ::std::integral char_type>
requires (::std::numeric_limits<::off_t>::is_signed &&
	::std::numeric_limits<::off_t>::digits >= ::std::numeric_limits<::fast_io::intfpos_t>::digits)
inline posix_operation_result posix_readadvise_nothrow(basic_posix_family_io_observer<family, char_type> file,
	::fast_io::intfpos_t offset, int byte_count) noexcept
{
	// Both signed arguments reach the real SDK unchanged. Do not pre-check
	// negatives or clamp ranges: the native descriptor error has its own order.
	// Clear the complete real SDK cell with fast_io before assigning fields;
	// [safe complete owned native radvisory cell]
	//       ^ ONE F_RDADVISE borrows this cell only until synchronous return.
	// No pointer advances, escapes, arbitrary command/payload is accepted, FD
	// ownership changes, allocation, retry or throwing adapter is introduced.
	struct ::radvisory native{};
	// [safe native byte representation, exactly sizeof(native) bytes]
	//       ^ fast_io clears this one owned cell; its returned end is unused.
	::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(__builtin_addressof(native)), sizeof(native));
	native.ra_offset = static_cast<::off_t>(offset);
	native.ra_count = byte_count;
	auto const result{details::posix_nothrow_abi::fd_read_advice(file.native_handle(), F_RDADVISE, __builtin_addressof(native))};
	if (result < 0) return {details::posix_nothrow_abi::last_error()};
	return {};
}
#endif
template <::fast_io::posix_family family, ::std::integral char_type>
inline posix_operation_result posix_close_nothrow(basic_posix_family_file<family, char_type>& file) noexcept
{
	// Release invalidates the unique persistent owner before one close. Retrying
	// EINTR could close a reused descriptor; destruction can never close it twice.
	int const descriptor{file.release()};
#if defined(__linux__) && defined(__NR_close)
	auto const result{details::sys_close(descriptor)};
	if (::fast_io::linux_system_call_fails(result)) return {static_cast<int>(-result)};
#else
	auto const result{details::posix_nothrow_abi::fd_close(descriptor)};
	if (result < 0) return {details::posix_nothrow_abi::last_error()};
#endif
	return {};
}
} // namespace fast_io
#endif
