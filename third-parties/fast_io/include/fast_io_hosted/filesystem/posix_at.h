﻿#pragma once

namespace fast_io
{

namespace posix
{
using posix_ssize_t = ::std::make_signed_t<::std::size_t>;

#ifdef __DARWIN_C_LEVEL
extern int libc_faccessat(int dirfd, char const *pathname, int mode, int flags) noexcept __asm__("_faccessat");
extern int libc_renameat(int olddirfd, char const *oldpath, int newdirfd, char const *newpath) noexcept
	__asm__("_renameat");
extern int libc_linkat(int olddirfd, char const *oldpath, int newdirfd, char const *newpath, int flags) noexcept
	__asm__("_linkat");
extern int libc_symlinkat(char const *oldpath, int newdirfd, char const *newpath) noexcept __asm__("_symlinkat");
extern int libc_fchmodat(int dirfd, char const *pathname, mode_t mode, int flags) noexcept __asm__("_fchmodat");
extern int libc_utimensat(int dirfd, char const *pathname, struct timespec const *times, int flags) noexcept
	__asm__("_utimensat");
extern int libc_fchownat(int dirfd, char const *pathname, uid_t owner, gid_t group, int flags) noexcept
	__asm__("_fchownat");
extern int libc_fstatat(int dirfd, char const *pathname, struct stat *buf, int flags) noexcept __asm__("_fstatat");
extern int libc_mkdirat(int dirfd, char const *pathname, mode_t mode) noexcept __asm__("_mkdirat");
extern int libc_mknodat(int dirfd, char const *pathname, mode_t mode, dev_t dev) noexcept __asm__("_mknodat");
extern int libc_unlinkat(int dirfd, char const *pathname, int flags) noexcept __asm__("_unlinkat");
extern posix_ssize_t libc_readlinkat(int dirfd, char const *pathname, char *buf, ::std::size_t bufsiz) noexcept __asm__("_readlinkat");
#else
extern int libc_faccessat(int dirfd, char const *pathname, int mode, int flags) noexcept __asm__("faccessat");
extern int libc_renameat(int olddirfd, char const *oldpath, int newdirfd, char const *newpath) noexcept
	__asm__("renameat");
extern int libc_linkat(int olddirfd, char const *oldpath, int newdirfd, char const *newpath, int flags) noexcept
	__asm__("linkat");
extern int libc_symlinkat(char const *oldpath, int newdirfd, char const *newpath) noexcept __asm__("symlinkat");
extern int libc_fchmodat(int dirfd, char const *pathname, mode_t mode, int flags) noexcept __asm__("fchmodat");
extern int libc_utimensat(int dirfd, char const *pathname, struct timespec const *times, int flags) noexcept
	__asm__("utimensat");
extern int libc_fchownat(int dirfd, char const *pathname, uid_t owner, gid_t group, int flags) noexcept
	__asm__("fchownat");
extern int libc_fstatat(int dirfd, char const *pathname, struct stat *buf, int flags) noexcept __asm__("fstatat");
extern int libc_mkdirat(int dirfd, char const *pathname, mode_t mode) noexcept __asm__("mkdirat");
extern int libc_mknodat(int dirfd, char const *pathname, mode_t mode, dev_t dev) noexcept __asm__("mknodat");
extern int libc_unlinkat(int dirfd, char const *pathname, int flags) noexcept __asm__("unlinkat");
extern posix_ssize_t libc_readlinkat(int dirfd, char const *pathname, char *buf, ::std::size_t bufsiz) noexcept __asm__("readlinkat");
#endif
} // namespace posix

enum class posix_at_flags
{
	eaccess =
#ifdef AT_EACCESS
		AT_EACCESS
#else
		0
#endif
		,
	symlink_nofollow =
#ifdef AT_SYMLINK_NOFOLLOW
		AT_SYMLINK_NOFOLLOW
#else
		0
#endif
		,
	no_automount =
#ifdef AT_NO_AUTOMOUNT
		AT_NO_AUTOMOUNT
#else
		0
#endif
		,
	removedir =
#ifdef AT_REMOVEDIR
		AT_REMOVEDIR
#else
		0
#endif
		,
	empty_path =
#ifdef AT_EMPTY_PATH
		AT_EMPTY_PATH
#else
		0x1000
#endif
};

using native_at_flags = posix_at_flags;

inline constexpr posix_at_flags operator&(posix_at_flags x, posix_at_flags y) noexcept
{
	using utype = typename ::std::underlying_type<posix_at_flags>::type;
	return static_cast<posix_at_flags>(static_cast<utype>(x) & static_cast<utype>(y));
}

inline constexpr posix_at_flags operator|(posix_at_flags x, posix_at_flags y) noexcept
{
	using utype = typename ::std::underlying_type<posix_at_flags>::type;
	return static_cast<posix_at_flags>(static_cast<utype>(x) | static_cast<utype>(y));
}

inline constexpr posix_at_flags operator^(posix_at_flags x, posix_at_flags y) noexcept
{
	using utype = typename ::std::underlying_type<posix_at_flags>::type;
	return static_cast<posix_at_flags>(static_cast<utype>(x) ^ static_cast<utype>(y));
}

inline constexpr posix_at_flags operator~(posix_at_flags x) noexcept
{
	using utype = typename ::std::underlying_type<posix_at_flags>::type;
	return static_cast<posix_at_flags>(~static_cast<utype>(x));
}

inline constexpr posix_at_flags &operator&=(posix_at_flags &x, posix_at_flags y) noexcept
{
	return x = x & y;
}

inline constexpr posix_at_flags &operator|=(posix_at_flags &x, posix_at_flags y) noexcept
{
	return x = x | y;
}

inline constexpr posix_at_flags &operator^=(posix_at_flags &x, posix_at_flags y) noexcept
{
	return x = x ^ y;
}

namespace details
{

inline void posix_renameat_impl(int olddirfd, char const *oldpath, int newdirfd, char const *newpath)
{
	system_call_throw_error(
#if defined(__linux__) && defined(__NR_renameat)
		system_call<__NR_renameat, int>
#else
		::fast_io::posix::libc_renameat
#endif
		(olddirfd, oldpath, newdirfd, newpath));
}

inline void posix_linkat_impl(int olddirfd, char const *oldpath, int newdirfd, char const *newpath, int flags)
{
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_linkat, int>
#else
		::fast_io::posix::libc_linkat
#endif
		(olddirfd, oldpath, newdirfd, newpath, flags));
}

template <posix_api_22 dsp, typename... Args>
inline auto posix22_api_dispatcher(int olddirfd, char const *oldpath, int newdirfd, char const *newpath, Args... args)
{
	if constexpr (dsp == posix_api_22::renameat)
	{
		static_assert(sizeof...(Args) == 0);
		posix_renameat_impl(olddirfd, oldpath, newdirfd, newpath);
	}
	else if constexpr (dsp == posix_api_22::linkat)
	{
		posix_linkat_impl(olddirfd, oldpath, newdirfd, newpath, args...);
	}
}

inline void posix_symlinkat_impl(char const *oldpath, int newdirfd, char const *newpath)
{
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_symlinkat, int>
#else
		::fast_io::posix::libc_symlinkat
#endif
		(oldpath, newdirfd, newpath));
}

template <posix_api_12 dsp, typename... Args>
inline auto posix12_api_dispatcher(char const *oldpath, int newdirfd, char const *newpath, Args...)
{
	if constexpr (dsp == posix_api_12::symlinkat)
	{
		posix_symlinkat_impl(oldpath, newdirfd, newpath);
	}
}

inline void posix_faccessat_impl(int dirfd, char const *pathname, int mode, int flags)
{
	system_call_throw_error(
#if defined(__linux__) && defined(__NR_faccessat2)
		system_call<__NR_faccessat2, int>(dirfd, pathname, mode, flags)
#elif defined(__linux__) && defined(__NR_faccessat)
		system_call<__NR_faccessat, int>(dirfd, pathname, mode)
#else
		::fast_io::posix::libc_faccessat(dirfd, pathname, mode, flags)
#endif
		);
}

#if defined(__wasi__) && !defined(__wasilibc_unmodified_upstream)
inline void posix_fchownat_impl(int, char const *, uintmax_t, uintmax_t, int)
{
	throw_posix_error(ENOTSUP);
}
#else
inline void posix_fchownat_impl(int dirfd, char const *pathname, uintmax_t owner, uintmax_t group, int flags)
{
	if constexpr (sizeof(uintmax_t) > sizeof(uid_t))
	{
		constexpr ::std::uintmax_t mx{::std::numeric_limits<uid_t>::max()};
		if (static_cast<::std::uintmax_t>(owner) > mx)
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	if constexpr (sizeof(uintmax_t) > sizeof(gid_t))
	{
		constexpr ::std::uintmax_t mx{::std::numeric_limits<gid_t>::max()};
		if (static_cast<::std::uintmax_t>(group) > mx)
		{
			throw_posix_error(EOVERFLOW);
		}
	}
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_fchownat, int>
#else
		::fast_io::posix::libc_fchownat
#endif
		(dirfd, pathname, static_cast<uid_t>(owner), static_cast<gid_t>(group), flags));
}
#endif

#if defined(__wasi__) && !defined(__wasilibc_unmodified_upstream)
inline void posix_fchmodat_impl(int, char const *, mode_t, int)
{
	throw_posix_error(ENOTSUP);
}
#else
inline void posix_fchmodat_impl(int dirfd, char const *pathname, mode_t mode, int flags)
{
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_fchmodat, int>
#else
		::fast_io::posix::libc_fchmodat
#endif
		(dirfd, pathname, mode, flags));
}
#endif

inline posix_file_status posix_fstatat_impl(int dirfd, char const *pathname, int flags)
{
#if defined(__linux__)

#if defined(__USE_LARGEFILE64) && (defined(__NR_newfstatat) || defined(__NR_fstatat64))
	struct stat64 buf;
#else
	struct stat buf;
#endif
#if defined(__NR_newfstatat) || defined(__NR_fstatat64) || defined(__NR_fstatat)
	system_call_throw_error(system_call<
#if defined(__NR_newfstatat)
							__NR_newfstatat
#elif defined(__NR_fstatat64)
							__NR_fstatat64
#else
							__NR_fstatat
#endif
							,
							int>(dirfd, pathname, __builtin_addressof(buf), flags));

#else
	if ((::fast_io::posix::libc_fstatat(dirfd, pathname, __builtin_addressof(buf), flags)) < 0)
	{
		throw_posix_error();
	}
#endif

#else
	struct stat buf;
	system_call_throw_error(::fast_io::posix::libc_fstatat(dirfd, pathname, __builtin_addressof(buf), flags));
#endif
	return ::fast_io::details::struct_stat_to_posix_file_status(buf);
}

inline void posix_mkdirat_impl(int dirfd, char const *pathname, mode_t mode)
{
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_mkdirat, int>
#else
		::fast_io::posix::libc_mkdirat
#endif
		(dirfd, pathname, mode));
}
#if 0
#if (defined(__wasi__) && !defined(__wasilibc_unmodified_upstream)) || defined(__DARWIN_C_LEVEL)
inline void posix_mknodat_impl(int, char const* , mode_t,::std::uintmax_t)
{
	throw_posix_error(ENOTSUP);
}
#else

inline void posix_mknodat_impl(int dirfd, char const* pathname, mode_t mode,::std::uintmax_t dev)
{
	if constexpr(sizeof(::std::uintmax_t)>sizeof(dev_t))
	{
		constexpr ::std::uintmax_t mx{::std::numeric_limits<dev_t>::max()};
		if(static_cast<::std::uintmax_t>(dev)>mx)
			throw_posix_error(EOVERFLOW);
	}
	system_call_throw_error(
#if defined(__linux__)
	system_call<
#if __NR_mknodat
	__NR_mknodat
#endif
	,int>
#else
	::fast_io::posix::libc_mknodat
#endif
	(dirfd,pathname,mode,static_cast<dev_t>(dev)));
}

#endif
#endif
inline void posix_unlinkat_impl(int dirfd, char const *path, int flags)
{
	system_call_throw_error(
#if defined(__linux__)
		system_call<__NR_unlinkat, int>
#else
		::fast_io::posix::libc_unlinkat
#endif
		(dirfd, path, flags));
}

namespace details
{
inline constexpr struct timespec unix_timestamp_to_struct_timespec(unix_timestamp stmp) noexcept
{
	constexpr ::std::uint_least64_t mul_factor{uint_least64_subseconds_per_second / 1000000000u};
	return {static_cast<::std::time_t>(stmp.seconds),
			static_cast<long>(static_cast<long unsigned>(stmp.subseconds / mul_factor))};
}

inline
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
	constexpr
#endif
	struct timespec
	unix_timestamp_to_struct_timespec([[maybe_unused]] unix_timestamp_option opt) noexcept
{
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
	switch (opt.flags)
	{
	case utime_flags::now:
		return {.tv_sec = 0, .tv_nsec = UTIME_NOW};
	case utime_flags::omit:
		return {.tv_sec = 0, .tv_nsec = UTIME_OMIT};
	default:
		return unix_timestamp_to_struct_timespec(opt.timestamp);
	}
#else
	throw_posix_error(EINVAL);
#endif
}

#if defined(__linux__) && defined(__NR_utimensat_time64)

// https://github.com/torvalds/linux/blob/07e27ad16399afcd693be20211b0dfae63e0615f/include/uapi/linux/time_types.h#L7
// https://github.com/qemu/qemu/blob/ab8008b231e758e03c87c1c483c03afdd9c02e19/linux-user/syscall_defs.h#L251
// https://github.com/bminor/glibc/blob/b7e0ec907ba94b6fcc6142bbaddea995bcc3cef3/include/struct___timespec64.h#L15
struct kernel_timespec64
{
	::std::int_least64_t tv_sec;
	::std::int_least64_t tv_nsec;
};

inline constexpr kernel_timespec64 unix_timestamp_to_struct_timespec64(unix_timestamp stmp) noexcept
{
	constexpr ::std::uint_least64_t mul_factor{uint_least64_subseconds_per_second / 1000000000u};
	return {static_cast<::std::int_least64_t>(stmp.seconds),
			static_cast<::std::int_least64_t>(stmp.subseconds / mul_factor)};
}

inline
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
	constexpr
#endif
    kernel_timespec64
	unix_timestamp_to_struct_timespec64([[maybe_unused]] unix_timestamp_option opt) noexcept
{
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
	switch (opt.flags)
	{
	case utime_flags::now:
		return {.tv_sec = 0, .tv_nsec = static_cast<::std::int_least64_t>(UTIME_NOW)};
	case utime_flags::omit:
		return {.tv_sec = 0, .tv_nsec = static_cast<::std::int_least64_t>(UTIME_OMIT)};
	default:
		return unix_timestamp_to_struct_timespec64(opt.timestamp);
	}
#else
	throw_posix_error(EINVAL);
#endif
}
#endif

} // namespace details

inline void posix_utimensat_impl(int dirfd, char const *path, unix_timestamp_option creation_time,
								 unix_timestamp_option last_access_time, unix_timestamp_option last_modification_time,
								 int flags)
{
	if (creation_time.flags != utime_flags::omit)
	{
		throw_posix_error(EINVAL);
	}

#if defined(__linux__) && defined(__NR_utimensat_time64)
    details::kernel_timespec64 ts[2]{
		details::unix_timestamp_to_struct_timespec64(last_access_time),
		details::unix_timestamp_to_struct_timespec64(last_modification_time),
	};
	details::kernel_timespec64 *tsptr{ts};
#else
	struct timespec ts[2]{
		details::unix_timestamp_to_struct_timespec(last_access_time),
		details::unix_timestamp_to_struct_timespec(last_modification_time),
	};
	struct timespec *tsptr{ts};
#endif

	system_call_throw_error(
#if defined(__linux__)
#if defined(__NR_utimensat_time64)
		system_call<__NR_utimensat_time64, int>
#elif defined(__NR_utimensat)
		system_call<__NR_utimensat, int>
#else
        ::fast_io::posix::libc_utimensat
#endif
#else
		::fast_io::posix::libc_utimensat
#endif
		(dirfd, path, tsptr, flags));
}

template<::std::integral char_type>
inline ::fast_io::details::basic_ct_string<char_type> posix_readlinkat_impl([[maybe_unused]] int dirfd, [[maybe_unused]] char const *pathname)
{
#if defined(AT_SYMLINK_NOFOLLOW)
    using posix_ssize_t = ::std::make_signed_t<::std::size_t>;

	// The standard POSIX API does not provide a direct interface to call readlink using an fd, so toctou cannot be avoided.

#if defined(__linux__)

#if defined(__USE_LARGEFILE64) && (defined(__NR_newfstatat) || defined(__NR_fstatat64))
	struct ::stat64 buf;
#else
	struct ::stat buf;
#endif
#if defined(__NR_newfstatat) || defined(__NR_fstatat64) || defined(__NR_fstatat)
	system_call_throw_error(system_call<
#if defined(__NR_newfstatat)
							__NR_newfstatat
#elif defined(__NR_fstatat64)
							__NR_fstatat64
#else
							__NR_fstatat
#endif
							,
							int>(dirfd, pathname, __builtin_addressof(buf), AT_SYMLINK_NOFOLLOW));

#else
	if ((::fast_io::posix::libc_fstatat(dirfd, pathname, __builtin_addressof(buf), AT_SYMLINK_NOFOLLOW)) < 0)
	{
		throw_posix_error();
	}
#endif

#else
	struct ::stat buf;
	system_call_throw_error(::fast_io::posix::libc_fstatat(dirfd, pathname, __builtin_addressof(buf),AT_SYMLINK_NOFOLLOW));
#endif

    auto const symlink_size{static_cast<::std::size_t>(buf.st_size)};

	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::details::basic_ct_string<char> result(symlink_size);

		posix_ssize_t readlink_bytes{
#if defined(__linux__) && defined(__NR_readlinkat)
							system_call<__NR_readlinkat, posix_ssize_t>(dirfd, pathname, result.data(), symlink_size)
#else
							static_cast<posix_ssize_t>(::fast_io::posix::libc_readlinkat(dirfd, pathname, result.data(), symlink_size))
#endif
						};
			
		system_call_throw_error(readlink_bytes);
			
		if(static_cast<::std::size_t>(readlink_bytes) != symlink_size)
		{
			throw_posix_error(EIO);
		}

		return result;
	}
	else 
	{
		local_operator_new_array_ptr<char> dynamic_buffer{symlink_size};

		posix_ssize_t readlink_bytes{
#if defined(__linux__) && defined(__NR_readlinkat)
							system_call<__NR_readlinkat, posix_ssize_t>(dirfd, pathname, dynamic_buffer.get(), symlink_size)
#else
							static_cast<posix_ssize_t>(::fast_io::posix::libc_readlinkat(dirfd, pathname, dynamic_buffer.get(), symlink_size))
#endif
						};
			
		system_call_throw_error(readlink_bytes);
			
		if(static_cast<::std::size_t>(readlink_bytes) != symlink_size) [[unlikely]]
		{
			throw_posix_error(EIO);
		}

		return ::fast_io::details::concat_ct<char_type>(::fast_io::mnp::code_cvt(::fast_io::mnp::strvw(dynamic_buffer.get(), dynamic_buffer.get() + symlink_size)));
	}
#else
	// Since faccessat also requires the AT_SYMLINK_NOFOLLOW flag, it can only result in an error.
	throw_posix_error(EINVAL);
#endif
}

template <posix_api_1x dsp, typename... Args>
inline auto posix1x_api_dispatcher(int dirfd, char const *path, Args... args)
{
	if constexpr (dsp == posix_api_1x::faccessat)
	{
		posix_faccessat_impl(dirfd, path, args...);
	}
	else if constexpr (dsp == posix_api_1x::fchownat)
	{
		posix_fchownat_impl(dirfd, path, args...);
	}
	else if constexpr (dsp == posix_api_1x::fchmodat)
	{
		posix_fchmodat_impl(dirfd, path, args...);
	}
	else if constexpr (dsp == posix_api_1x::fstatat)
	{
		return posix_fstatat_impl(dirfd, path, args...);
	}
	else if constexpr (dsp == posix_api_1x::mkdirat)
	{
		posix_mkdirat_impl(dirfd, path, args...);
	}
#if 0
	else if constexpr(dsp==posix_api_1x::mknodat)
		posix_mknodat_impl(dirfd,path,args...);
#endif
	else if constexpr (dsp == posix_api_1x::unlinkat)
	{
		posix_unlinkat_impl(dirfd, path, args...);
	}
	else if constexpr (dsp == posix_api_1x::utimensat)
	{
		posix_utimensat_impl(dirfd, path, args...);
	}
}

template <::std::integral char_type, posix_api_ct dsp, typename... Args>
inline auto posixct_api_dispatcher(int dirfd, char const *path, Args... args)
{
    if constexpr (dsp == posix_api_ct::readlinkat)
    {
        return posix_readlinkat_impl<char_type>(dirfd, path, args...);
    }
}

template <posix_api_22 dsp, ::fast_io::constructible_to_os_c_str old_path_type,
		  ::fast_io::constructible_to_os_c_str new_path_type, typename... Args>
inline auto posix_deal_with22(int olddirfd, old_path_type const &oldpath, int newdirfd, new_path_type const &newpath, Args... args)
{
	return fast_io::posix_api_common(
		oldpath,
		[&](char const *oldpath_c_str) {
			return fast_io::posix_api_common(
				newpath, [&](char const *newpath_c_str) { return posix22_api_dispatcher<dsp>(olddirfd, oldpath_c_str, newdirfd, newpath_c_str, args...); });
		});
}

template <posix_api_12 dsp, ::fast_io::constructible_to_os_c_str old_path_type,
		  ::fast_io::constructible_to_os_c_str new_path_type>
inline auto posix_deal_with12(old_path_type const &oldpath, int newdirfd, new_path_type const &newpath)
{
	return fast_io::posix_api_common(
		oldpath,
		[&](char const *oldpath_c_str) {
			return fast_io::posix_api_common(
				newpath, [&](char const *newpath_c_str) { return posix12_api_dispatcher<dsp>(oldpath_c_str, newdirfd, newpath_c_str); });
		});
}

template <posix_api_1x dsp, ::fast_io::constructible_to_os_c_str path_type, typename... Args>
inline auto posix_deal_with1x(int dirfd, path_type const &path, Args... args)
{
	return fast_io::posix_api_common(path, [&](char const *path_c_str) { return posix1x_api_dispatcher<dsp>(dirfd, path_c_str, args...); });
}

template <::std::integral char_type, posix_api_ct dsp, ::fast_io::constructible_to_os_c_str path_type, typename... Args>
inline auto posix_deal_withct(int dirfd, path_type const &path, Args... args)
{
    return fast_io::posix_api_common(path, [&](char const *path_c_str) { return posixct_api_dispatcher<char_type, dsp>(dirfd, path_c_str, args...); });
}

} // namespace details

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void posix_renameat(posix_at_entry oldent, old_path_type const &oldpath, posix_at_entry newent,
						   new_path_type const &newpath)
{
	details::posix_deal_with22<details::posix_api_22::renameat>(oldent.fd, oldpath, newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str new_path_type>
inline void posix_renameat(posix_fs_dirent fs_dirent, posix_at_entry newent, new_path_type const &newpath)
{
	details::posix_deal_with22<details::posix_api_22::renameat>(
		fs_dirent.fd, ::fast_io::manipulators::os_c_str(fs_dirent.filename), newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void posix_symlinkat(old_path_type const &oldpath, posix_at_entry newent, new_path_type const &newpath)
{
	details::posix_deal_with12<details::posix_api_12::symlinkat>(oldpath, newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void native_renameat(posix_at_entry oldent, old_path_type const &oldpath, posix_at_entry newent,
							new_path_type const &newpath)
{
	details::posix_deal_with22<details::posix_api_22::renameat>(oldent.fd, oldpath, newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str new_path_type>
inline void native_renameat(posix_fs_dirent fs_dirent, posix_at_entry newent, new_path_type const &newpath)
{
	details::posix_deal_with22<details::posix_api_22::renameat>(
		fs_dirent.fd, ::fast_io::manipulators::os_c_str(fs_dirent.filename), newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void native_symlinkat(old_path_type const &oldpath, posix_at_entry newent, new_path_type const &newpath)
{
	details::posix_deal_with12<details::posix_api_12::symlinkat>(oldpath, newent.fd, newpath);
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_faccessat(posix_at_entry ent, path_type const &path, access_how mode,
							posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::faccessat>(ent.fd, path, static_cast<int>(mode),
																 static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_faccessat(posix_at_entry ent, path_type const &path, access_how mode,
							 posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::faccessat>(ent.fd, path, static_cast<int>(mode),
																 static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_fchmodat(posix_at_entry ent, path_type const &path, perms mode,
						   posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::fchmodat>(ent.fd, path, static_cast<int>(mode),
																static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_fchmodat(posix_at_entry ent, path_type const &path, perms mode,
							posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::fchmodat>(ent.fd, path, static_cast<int>(mode),
																static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_fchownat(posix_at_entry ent, path_type const &path, ::std::uintmax_t owner, ::std::uintmax_t group,
						   posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::fchownat>(ent.fd, path, owner, group, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_fchownat(posix_at_entry ent, path_type const &path, ::std::uintmax_t owner, ::std::uintmax_t group,
							posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::fchownat>(ent.fd, path, owner, group, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline posix_file_status posix_fstatat(posix_at_entry ent, path_type const &path,
									   posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	return details::posix_deal_with1x<details::posix_api_1x::fstatat>(ent.fd, path, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline posix_file_status native_fstatat(posix_at_entry ent, path_type const &path,
										posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	return details::posix_deal_with1x<details::posix_api_1x::fstatat>(ent.fd, path, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_mkdirat(posix_at_entry ent, path_type const &path, perms perm = static_cast<perms>(509))
{
	return details::posix_deal_with1x<details::posix_api_1x::mkdirat>(ent.fd, path, static_cast<mode_t>(perm));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_mkdirat(posix_at_entry ent, path_type const &path, perms perm = static_cast<perms>(509))
{
	return details::posix_deal_with1x<details::posix_api_1x::mkdirat>(ent.fd, path, static_cast<mode_t>(perm));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_unlinkat(posix_at_entry ent, path_type const &path, posix_at_flags flags = {})
{
	details::posix_deal_with1x<details::posix_api_1x::unlinkat>(ent.fd, path, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_unlinkat(posix_at_entry ent, path_type const &path, posix_at_flags flags = {})
{
	details::posix_deal_with1x<details::posix_api_1x::unlinkat>(ent.fd, path, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void posix_linkat(posix_at_entry oldent, old_path_type const &oldpath, posix_at_entry newent,
						 new_path_type const &newpath, posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with22<details::posix_api_22::linkat>(oldent.fd, oldpath, newent.fd, newpath,
															  static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str old_path_type, ::fast_io::constructible_to_os_c_str new_path_type>
inline void native_linkat(posix_at_entry oldent, old_path_type const &oldpath, posix_at_entry newent,
						  new_path_type const &newpath, posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with22<details::posix_api_22::linkat>(oldent.fd, oldpath, newent.fd, newpath,
															  static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void posix_utimensat(posix_at_entry ent, path_type const &path, unix_timestamp_option creation_time,
							unix_timestamp_option last_access_time, unix_timestamp_option last_modification_time,
							posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::utimensat>(ent.fd, path, creation_time, last_access_time,
																 last_modification_time, static_cast<int>(flags));
}

template <::fast_io::constructible_to_os_c_str path_type>
inline void native_utimensat(posix_at_entry ent, path_type const &path, unix_timestamp_option creation_time,
							 unix_timestamp_option last_access_time, unix_timestamp_option last_modification_time,
							 posix_at_flags flags = posix_at_flags::symlink_nofollow)
{
	details::posix_deal_with1x<details::posix_api_1x::utimensat>(ent.fd, path, creation_time, last_access_time,
																 last_modification_time, static_cast<int>(flags));
}

// ct
template <::std::integral char_type, ::fast_io::constructible_to_os_c_str path_type>
inline ::fast_io::details::basic_ct_string<char_type> posix_readlinkat(posix_at_entry ent, path_type const &path)
{
    return details::posix_deal_withct<char_type, details::posix_api_ct::readlinkat>(ent.fd, path);
}

template <::std::integral char_type, ::fast_io::constructible_to_os_c_str path_type>
inline ::fast_io::details::basic_ct_string<char_type> native_readlinkat(posix_at_entry ent, path_type const &path)
{
    return details::posix_deal_withct<char_type, details::posix_api_ct::readlinkat>(ent.fd, path);
}

#if 0
template<::fast_io::constructible_to_os_c_str path_type>
inline void posix_mknodat(posix_at_entry ent,path_type const& path,perms perm,::std::uintmax_t dev)
{
	return details::posix_deal_with1x<details::posix_api_1x::mknodat>(ent.fd,path,static_cast<mode_t>(perm),dev);
}

template<::fast_io::constructible_to_os_c_str path_type>
inline void native_mknodat(posix_at_entry ent,path_type const& path,perms perm,::std::uintmax_t dev)
{
	return details::posix_deal_with1x<details::posix_api_1x::mknodat>(ent.fd,path,static_cast<mode_t>(perm),dev);
}
#endif

#if 0
template<::std::integral ch_type>
struct basic_posix_readlinkat_t
{
	using char_type = ch_type;
	int dirfd{-1};
	char const* path{};
};

template<::fast_io::constructible_to_os_c_str path_type,::std::integral char_type>
inline constexpr auto posix_readlinkat(
	posix_at_entry ent,
	path_type const& path,
	char_type* buf,::std::size_t bufsz) noexcept
{
	return basic_posix_readlinkat_t<value_type>{ent.fd,strvw};
}

template<::fast_io::constructible_to_os_c_str path_type>
inline constexpr auto native_readlinkat(
	posix_at_entry ent,
	path_type const& path) noexcept
{
	auto strvw{path};
	using value_type = typename ::std::remove_cvref_t<decltype(strvw)>::value_type;
	if constexpr(sizeof(value_type)==1)
		return basic_posix_readlinkat_t<value_type>{ent.fd,strvw.c_str()};
	else
		return basic_posix_readlinkat_t<value_type>{ent.fd,strvw};
}


namespace details
{

inline constexpr ::std::size_t read_linkbuffer_size() noexcept
{
#if defined(PATH_MAX)
	if constexpr(PATH_MAX<4096)
		return 4096;
	else
		return PATH_MAX;
#else
	return 4096;
#endif
}

inline ::std::size_t posix_readlinkat_common_impl(int dirfd,char const* pathname,char* buffer)
{
	constexpr ::std::size_t buffer_size{read_linkbuffer_size()};
	::std::ptrdiff_t bytes{
#if defined(__linux__)
	system_call<
#if __NR_readlinkat
	__NR_readlinkat
#endif
	,int>
#else
	::fast_io::posix::libc_readlinkat
#endif
	(dirfd,pathname,buffer,buffer_size)
	};
	system_call_throw_error(bytes);
	return static_cast<::std::size_t>(bytes);
}

template<::std::integral path_char_type>
inline ::std::size_t read_linkat_impl_phase2(char* dst,basic_posix_readlinkat_t<path_char_type> rlkat)
{
	if constexpr(sizeof(path_char_type)==1)
	{
		return posix_readlinkat_common_impl(rlkat.dirfd,reinterpret_cast<char const*>(rlkat.path),dst);
	}
	else
	{
		posix_api_encoding_converter converter(rlkat.path.data(),rlkat.path.size());
		return posix_readlinkat_common_impl(rlkat.dirfd,converter.native_c_str(),dst);
	}
}

template<
::std::integral to_char_type,
::std::integral path_char_type>
inline to_char_type* read_linkat_impl_phase1(to_char_type* dst,basic_posix_readlinkat_t<path_char_type> rlkat)
{
	if constexpr(sizeof(path_char_type)==1)
	{
		return dst+read_linkat_impl_phase2(reinterpret_cast<char*>(dst),rlkat);
	}
	else
	{
		constexpr ::std::size_t buffer_size{read_linkbuffer_size()};
		local_operator_new_array_ptr<char8_t> dynamic_buffer(buffer_size);
		::std::size_t bytes{read_linkat_impl_phase2(reinterpret_cast<char*>(dynamic_buffer.ptr),rlkat)};
		return codecvt::general_code_cvt_full(dynamic_buffer.ptr,dynamic_buffer.ptr+bytes,dst);
	}
}

}


template<::std::integral char_type,::std::integral path_char_type>
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type,basic_posix_readlinkat_t<path_char_type>>,
	basic_posix_readlinkat_t<path_char_type>) noexcept
{
	if constexpr(sizeof(char_type)==1)
		return details::read_linkbuffer_size();
	else
	{
		constexpr ::std::size_t sz{details::read_linkbuffer_size()};
		constexpr ::std::size_t decorated_size{details::cal_decorated_reserve_size<1,sizeof(char_type)>(sz)};
		return decorated_size;
	}
}

template<::std::integral char_type,::std::integral char_type>
inline constexpr char_type* print_reserve_define(io_reserve_type_t<char_type,
	basic_posix_readlinkat_t<char_type>>,
	char_type* iter,
	basic_posix_readlinkat_t<char_type> rlkat)
{
	return details::read_linkat_impl_phase1(iter,rlkat);
}
#endif

} // namespace fast_io
