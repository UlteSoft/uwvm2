

/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <cstring>
# include <limits>
# include <concepts>
# include <bit>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(__MSDOS__) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#  include <unistd.h>
#  include <errno.h>
#  include <sys/stat.h>
#  include <sys/time.h>
# endif
# if defined(__MSDOS__) || defined(__DJGPP__)
#  include <utime.h>
# endif
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
# include "base.h"
# include "posix.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t set_dir_time(::fast_io::dir_file & dirfile,
                                                                      ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim,
                                                                      ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim,
                                                                      ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags) noexcept
    {
        [[maybe_unused]] auto const& file_fd{dirfile};

#if defined(_WIN32) && !defined(__CYGWIN__)
// Windows
# if !defined(_WIN32_WINDOWS)
        // Windows NT

        auto const& curr_fd_native_file{file_fd};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        constexpr ::std::uint_least64_t gap{static_cast<::std::uint_least64_t>(11644473600000ULL) * 10000ULL};

        // Since WASI uses NSS time starting from 1970, while Win32 uses 0.1 microsecond time starting from 1601, the conversion requires no maximum
        // value checks whatsoever.
        ::std::uint_least64_t const atim_win32time{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 100u + gap};
        ::std::uint_least64_t const mtim_win32time{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 100u + gap};

        ::std::uint_least64_t lpLastAccessTime{};  // omit default
        ::std::uint_least64_t lpLastWriteTime{};   // omit default

        ::std::uint_least64_t currtime{};  // zero default

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
        {
#  if (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0602)
            // Windows 8 or later
            currtime = static_cast<::std::uint_least64_t>(::fast_io::win32::nt::RtlGetSystemTimePrecise());
#  else
            ::fast_io::win32::filetime ftm;  // no initialize
            ::fast_io::win32::GetSystemTimeAsFileTime(::std::addressof(ftm));
            currtime = (static_cast<::std::uint_least64_t>(ftm.dwHighDateTime) << 32u) | ftm.dwLowDateTime;
#  endif

            lpLastAccessTime = currtime;
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
        {
            lpLastAccessTime = atim_win32time;
        }

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
        {
            // If not previously obtained, obtain it this time.
            if(!currtime) [[unlikely]]
            {
#  if (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0602)
                // Windows 8 or later
                currtime = static_cast<::std::uint_least64_t>(::fast_io::win32::nt::RtlGetSystemTimePrecise());
#  else
                ::fast_io::win32::filetime ftm;  // no initialize
                ::fast_io::win32::GetSystemTimeAsFileTime(::std::addressof(ftm));
                currtime = (static_cast<::std::uint_least64_t>(ftm.dwHighDateTime) << 32u) | ftm.dwLowDateTime;
#  endif
            }

            lpLastWriteTime = currtime;
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
        {
            lpLastWriteTime = mtim_win32time;
        }

        // file_basic_information passing 0 indicates no modification to the attribute
        ::fast_io::win32::nt::file_basic_information fbi{.CreationTime = {},
                                                         .LastAccessTime = lpLastAccessTime,
                                                         .LastWriteTime = lpLastWriteTime,
                                                         .ChangeTime = {},
                                                         .FileAttributes = {}};
        ::fast_io::win32::nt::io_status_block isb;  // no initialize
        constexpr bool zw{false};
        auto const status{::fast_io::win32::nt::nt_set_information_file<zw>(curr_fd_native_handle,
                                                                            ::std::addressof(isb),
                                                                            ::std::addressof(fbi),
                                                                            static_cast<::std::uint_least32_t>(sizeof(fbi)),
                                                                            ::fast_io::win32::nt::file_information_class::FileBasicInformation)};

        switch(status)
        {
            [[likely]] case 0x00000000uz /*STATUS_SUCCESS*/:
                break;
            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
            case 0xC0000008uz /*STATUS_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            case 0xC0000022uz /*STATUS_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
            case 0xC0000002uz /*STATUS_NOT_IMPLEMENTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
            case 0xC00000BBuz /*STATUS_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
            case 0xC00000A2uz /*STATUS_MEDIA_WRITE_PROTECTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
            case 0xC000000Duz /*STATUS_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            case 0xC0000185uz /*STATUS_IO_DEVICE_ERROR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            case 0xC0000001uz /*STATUS_UNSUCCESSFUL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            case 0xC00000E8uz /*STATUS_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

# else
        // Windows 9x
        // For Windows 9x, it is impossible to modify a directory's timestamp via the Win32 API and directory name without directly manipulating FAT32
        // or FAT16.
        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
# endif

#elif defined(__MSDOS__) || defined(__DJGPP__)
        // MSDOS-DJGPP
        auto const& curr_fd_native_file{file_fd};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        // Since MS-DOS can obtain the file descriptor name and then call utime, this can be implemented here.
        auto const fd_native_handle_pathname_cstr{::fast_io::noexcept_call(::__get_fd_name, curr_fd_native_handle)};

        if(fd_native_handle_pathname_cstr == nullptr) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

        ::fast_io::unix_timestamp omit_atim_unix_timestamp{};
        ::fast_io::unix_timestamp omit_mtim_unix_timestamp{};
        ::fast_io::unix_timestamp now_unix_timestamp{};

        // Note that, as under DOS a file only has a single timestamp, the actime field of struct utimbuf is ignored by this function, and only modtime
        // field is used. On filesystems which support long filenames, both fields are used and both access and modification times are set.
        struct utimbuf timestamp_spec;

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
        {
            if(now_unix_timestamp.seconds == 0u && now_unix_timestamp.subseconds == 0u)
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    now_unix_timestamp = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
# endif
            }

            // utime uses time_t seconds; assign seconds (ns truncated).
            timestamp_spec.actime = static_cast<::std::time_t>(now_unix_timestamp.seconds);
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
        {
            auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};

            if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
            {
                if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
            }

            // utime uses time_t seconds; convert ns to seconds.
            timestamp_spec.actime = static_cast<::std::time_t>(atim_seconds);
        }
        else
        {
            if(omit_atim_unix_timestamp.seconds == 0u && omit_atim_unix_timestamp.subseconds == 0u)
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    auto const tmp_status{status(curr_fd_native_file)};
                    omit_atim_unix_timestamp = tmp_status.atim;
                    omit_mtim_unix_timestamp = tmp_status.mtim;
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
# endif
            }

            // No need to check, because it will never overflow.
            // utime uses time_t seconds; assign preserved seconds.
            timestamp_spec.actime = static_cast<::std::time_t>(omit_atim_unix_timestamp.seconds);
        }

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
        {
            if(now_unix_timestamp.seconds == 0u && now_unix_timestamp.subseconds == 0u)
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    now_unix_timestamp = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
# endif
            }

            // utime uses time_t seconds; assign seconds (ns truncated).
            timestamp_spec.modtime = static_cast<::std::time_t>(now_unix_timestamp.seconds);
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
        {
            auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};

            if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
            {
                if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
            }

            // utime uses time_t seconds; convert ns to seconds.
            timestamp_spec.modtime = static_cast<::std::time_t>(mtim_seconds);
        }
        else
        {
            if(omit_mtim_unix_timestamp.seconds == 0u && omit_mtim_unix_timestamp.subseconds == 0u)
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    auto const tmp_status{status(curr_fd_native_file)};
                    omit_atim_unix_timestamp = tmp_status.atim;
                    omit_mtim_unix_timestamp = tmp_status.mtim;
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
# endif
            }

            // No need to check, because it will never overflow.
            // utime uses time_t seconds; assign preserved seconds.
            timestamp_spec.modtime = static_cast<::std::time_t>(omit_mtim_unix_timestamp.seconds);
        }

        // Note that, as under DOS a file only has a single timestamp, the actime field of struct utimbuf is ignored by this function, and only modtime
        // field is used. On filesystems which support long filenames, both fields are used and both access and modification times are set.
        if(::uwvm2::imported::wasi::wasip1::func::posix::utime(fd_native_handle_pathname_cstr, ::std::addressof(timestamp_spec)) == -1) [[unlikely]]
        {
            switch(errno)
            {
                // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
# if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
# if defined(ENOTSUP)
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

#elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(__MSDOS__) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
// posix
# if defined(__linux__)
        // linux
        auto const& curr_fd_native_file{file_fd};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        if constexpr(::std::numeric_limits<::std::size_t>::max() >= ::std::numeric_limits<::std::uint_least64_t>::max())
        {
            // 64 bit
            // All fields in the 64-bit timespec are 64-bit fields.
            struct timespec timestamp_spec[2];

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
            {
                auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
            }
            else
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
            }

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
            {
                auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
            }
            else
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
            }

#  if defined(__NR_utimensat)
            auto const res{::fast_io::system_call<__NR_utimensat, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
            if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
            {
                auto const err{static_cast<int>(-res)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
#  else
            if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
            {
                switch(errno)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
#  endif
        }
        else if constexpr(::std::numeric_limits<::std::size_t>::max() >= ::std::numeric_limits<::std::uint_least32_t>::max())
        {
            // 32bit

#  if defined(__NR_utimensat_time64)
            ::uwvm2::imported::wasi::wasip1::func::posix::linux_kernel_timespec64 timestamp_spec[2];

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
            {
                timestamp_spec[0] = {{}, static_cast<::std::int_least64_t>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
            {
                timestamp_spec[0] = {
                    static_cast<::std::int_least64_t>(static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u),
                    static_cast<::std::int_least64_t>(static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u)};
            }
            else
            {
                timestamp_spec[0] = {{}, static_cast<::std::int_least64_t>(UTIME_OMIT)};
            }

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
            {
                timestamp_spec[1] = {{}, static_cast<::std::int_least64_t>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
            {
                timestamp_spec[1] = {
                    static_cast<::std::int_least64_t>(static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u),
                    static_cast<::std::int_least64_t>(static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u)};
            }
            else
            {
                timestamp_spec[1] = {{}, static_cast<::std::int_least64_t>(UTIME_OMIT)};
            }

            auto const res{::fast_io::system_call<__NR_utimensat_time64, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
            if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
            {
                auto const err{static_cast<int>(-res)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }

#  else

            struct timespec timestamp_spec[2];

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
            {
                auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
            }
            else
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
            }

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
            {
                auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
            }
            else
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
            }

#   if defined(__NR_utimensat)
            auto const res{::fast_io::system_call<__NR_utimensat, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
            if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
            {
                auto const err{static_cast<int>(-res)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
#    if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
#   else
            if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
            {
                switch(errno)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
#    if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
#   endif
#  endif
        }
        else
        {
            // unknown

            struct timespec timestamp_spec[2];

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
            {
                auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
            }
            else
            {
                timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
            }

            if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
               ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
            }
            else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                    ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
            {
                auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                {
                    if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
            }
            else
            {
                timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
            }

            if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
            {
                switch(errno)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                    // instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
#  if defined(ENOTSUP)
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
        }
# elif !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 200809L
        // POSIX 2008

        auto const& curr_fd_native_file{file_fd};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        struct timespec timestamp_spec[2];

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
        {
            timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
        {
            auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
            auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

            if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
            {
                if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
            }

            timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
        }
        else
        {
            timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
        }

        if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
        {
            timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
        }
        else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
        {
            auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
            auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

            if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
            {
                if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow; }
            }

            timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
        }
        else
        {
            timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
        }

        if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
        {
            switch(errno)
            {
                // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
#  if defined(ENOTSUP)
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }
# else
        // POSIX.1-2008 formally introduced the ability to modify a file's nanosecond-precision time directly based on the file descriptor.
        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
# endif
#else
        // unsupported platform
        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
#endif

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    /// @brief     WasiPreview1.fd_filestat_set_times
    /// @details   __wasi_errno_t fd_filestat_set_times(__wasi_fd_t fd, __wasi_timestamp_t atim, __wasi_timestamp_t mtim, __wasi_fstflags_t fstflags);
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_filestat_set_times_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim,
        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags) noexcept
    {
        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // The pointer to `wasm_fd` is fixed and remains unchanged even when the vector within `fd_manager` is resized.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        // Subsequent operations involving the file descriptor require locking. curr_fd_release_guard release when return.
        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

        {
            // Prevent operations to obtain the size or perform resizing at this time.
            // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
            // Since the file descriptor's location is fixed and accessed via the unique pointer,

            // Simply acquiring data using a shared_lock
            ::uwvm2::utils::mutex::rw_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // Negative states have been excluded, so the conversion result will only be positive numbers.
            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(fd)};

            // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }
            }

            auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                }
            }
            else
            {
                // The addition here is safe.
                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
            }

            // curr_wasi_fd_t_p never nullptr
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
            {
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
#endif

            // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd vector). If the current thread
            // is performing, no other thread can be executing any close operations simultaneously, eliminating any destruction issues. Therefore,
            // acquiring the lock at this point is safe. However, the problem arises when, immediately after acquiring the lock and before releasing the manager
            // lock and beginning fd operations, another thread executes a deletion that removes this fd. Subsequent operations by the current thread would then
            // encounter issues. Thus, locking must occur before releasing fds_rwlock.
            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
            curr_fd_release_guard.lock();

            // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.
        }

        // curr_fd_uniptr is not null.
        auto& curr_fd{*curr_wasi_fd_t_p};

        // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether it is closed.
        // Therefore, a unified check is implemented.
        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        // Rights check: fd_filestat_set_times requires right_fd_filestat_set_times.
        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_filestat_set_times) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_filestat_set_times) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        // Setting both atim and atim_now (or mtim and mtim_now) should return einval.
        if(((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now &&
            (fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ||
           ((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now &&
            (fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
        }

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
            {
                [[maybe_unused]] auto& file_fd{
#if defined(_WIN32) && !defined(__CYGWIN__)
                    curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file
#else
                    curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd
#endif
                };

#if defined(_WIN32) && !defined(__CYGWIN__)
                // windows
                auto const& curr_fd_native_file{file_fd};
                auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

                constexpr ::std::uint_least64_t gap{static_cast<::std::uint_least64_t>(11644473600000ULL) * 10000ULL};

                // Since WASI uses NSS time starting from 1970, while Win32 uses 0.1 microsecond time starting from 1601, the conversion requires no maximum
                // value checks whatsoever.
                ::std::uint_least64_t const atim_win32time{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 100u + gap};
                ::std::uint_least64_t const mtim_win32time{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 100u + gap};

                ::std::uint_least64_t lpLastAccessTime{};  // omit default
                ::std::uint_least64_t lpLastWriteTime{};   // omit default

                ::std::uint_least64_t currtime{};  // zero default

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                {

# if (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0602) && !defined(_WIN32_WINDOWS)
                    // Windows 8 or later
                    currtime = static_cast<::std::uint_least64_t>(::fast_io::win32::nt::RtlGetSystemTimePrecise());
# else
                    ::fast_io::win32::filetime ftm;  // no initialize
                    ::fast_io::win32::GetSystemTimeAsFileTime(::std::addressof(ftm));
                    currtime = (static_cast<::std::uint_least64_t>(ftm.dwHighDateTime) << 32u) | ftm.dwLowDateTime;
# endif

                    lpLastAccessTime = currtime;
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                {
                    lpLastAccessTime = atim_win32time;
                }

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                {
                    // If not previously obtained, obtain it this time.
                    if(!currtime) [[unlikely]]
                    {
# if (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0602) && !defined(_WIN32_WINDOWS)
                        // Windows 8 or later
                        currtime = static_cast<::std::uint_least64_t>(::fast_io::win32::nt::RtlGetSystemTimePrecise());
# else
                        ::fast_io::win32::filetime ftm;  // no initialize
                        ::fast_io::win32::GetSystemTimeAsFileTime(::std::addressof(ftm));
                        currtime = (static_cast<::std::uint_least64_t>(ftm.dwHighDateTime) << 32u) | ftm.dwLowDateTime;
# endif
                    }

                    lpLastWriteTime = currtime;
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                {
                    lpLastWriteTime = mtim_win32time;
                }

# if !defined(_WIN32_WINDOWS)
                // Windows NT
                // file_basic_information passing 0 indicates no modification to the attribute
                ::fast_io::win32::nt::file_basic_information fbi{.CreationTime = {},
                                                                 .LastAccessTime = lpLastAccessTime,
                                                                 .LastWriteTime = lpLastWriteTime,
                                                                 .ChangeTime = {},
                                                                 .FileAttributes = {}};
                ::fast_io::win32::nt::io_status_block isb;  // no initialize
                constexpr bool zw{false};
                auto const status{::fast_io::win32::nt::nt_set_information_file<zw>(curr_fd_native_handle,
                                                                                    ::std::addressof(isb),
                                                                                    ::std::addressof(fbi),
                                                                                    static_cast<::std::uint_least32_t>(sizeof(fbi)),
                                                                                    ::fast_io::win32::nt::file_information_class::FileBasicInformation)};

                switch(status)
                {
                    [[likely]] case 0x00000000uz /*STATUS_SUCCESS*/:
                        break;
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case 0xC0000008uz /*STATUS_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC0000022uz /*STATUS_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case 0xC0000002uz /*STATUS_NOT_IMPLEMENTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    case 0xC00000BBuz /*STATUS_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case 0xC00000A2uz /*STATUS_MEDIA_WRITE_PROTECTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case 0xC000000Duz /*STATUS_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 0xC0000185uz /*STATUS_IO_DEVICE_ERROR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC0000001uz /*STATUS_UNSUCCESSFUL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC00000E8uz /*STATUS_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

# else
                // Windows 9x
                ::fast_io::win32::filetime const LastAccessTime_filetime{::std::bit_cast<::fast_io::win32::filetime>(lpLastAccessTime)};
                ::fast_io::win32::filetime const LastWriteTime_filetime{::std::bit_cast<::fast_io::win32::filetime>(lpLastWriteTime)};
                bool const set_atime{((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                                      ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ||
                                     ((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                                      ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)};
                bool const set_mtime{((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                                      ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ||
                                     ((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                                      ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)};
                ::fast_io::win32::filetime const* const pAccess{set_atime ? ::std::addressof(LastAccessTime_filetime) : nullptr};
                ::fast_io::win32::filetime const* const pWrite{set_mtime ? ::std::addressof(LastWriteTime_filetime) : nullptr};

                if(pAccess || pWrite)
                {
                    if(!::fast_io::win32::SetFileTime(curr_fd_native_handle, nullptr, pAccess, pWrite)) [[unlikely]]
                    {
                        // Not all file systems can record creation and last access times, and not all file systems record them in the same way. For example, on
                        // FAT, the resolution for creation time is 10 milliseconds, for write time it is 2 seconds, and for access time it is 1 day (access
                        // date). NTFS delays updating the last access time of a file for up to one hour after the last access.

                        switch(::fast_io::win32::GetLastError())
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case 6uz /*ERROR_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case 5uz /*ERROR_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case 1uz /*ERROR_INVALID_FUNCTION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            case 50uz /*ERROR_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                            case 19uz /*ERROR_WRITE_PROTECT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                            case 87uz /*ERROR_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case 1117uz /*ERROR_IO_DEVICE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case 31uz /*ERROR_GEN_FAILURE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case 1784uz /*ERROR_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
                }

# endif

#elif defined(__MSDOS__) || defined(__DJGPP__)
                // MSDOS-DJGPP
                auto const& curr_fd_native_file{file_fd};
                auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

                // Since MS-DOS can obtain the file descriptor name and then call utime, this can be implemented here.
                auto const fd_native_handle_pathname_cstr{::fast_io::noexcept_call(::__get_fd_name, curr_fd_native_handle)};

                if(fd_native_handle_pathname_cstr == nullptr) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                ::fast_io::unix_timestamp omit_atim_unix_timestamp{};
                ::fast_io::unix_timestamp omit_mtim_unix_timestamp{};
                ::fast_io::unix_timestamp now_unix_timestamp{};

                // Note that, as under DOS a file only has a single timestamp, the actime field of struct utimbuf is ignored by this function, and only modtime
                // field is used. On filesystems which support long filenames, both fields are used and both access and modification times are set.
                struct utimbuf timestamp_spec;

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                {
                    if(now_unix_timestamp.seconds == 0u && now_unix_timestamp.subseconds == 0u)
                    {
# ifdef UWVM_CPP_EXCEPTIONS
                        try
# endif
                        {
                            now_unix_timestamp = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime);
                        }
# ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error)
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
# endif
                    }

                    // utime uses time_t seconds; assign seconds (ns truncated).
                    timestamp_spec.actime = static_cast<::std::time_t>(now_unix_timestamp.seconds);
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                {
                    auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};

                    if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                    {
                        if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    // utime uses time_t seconds; convert ns to seconds.
                    timestamp_spec.actime = static_cast<::std::time_t>(atim_seconds);
                }
                else
                {
                    if(omit_atim_unix_timestamp.seconds == 0u && omit_atim_unix_timestamp.subseconds == 0u)
                    {
# ifdef UWVM_CPP_EXCEPTIONS
                        try
# endif
                        {
                            auto const tmp_status{status(curr_fd_native_file)};
                            omit_atim_unix_timestamp = tmp_status.atim;
                            omit_mtim_unix_timestamp = tmp_status.mtim;
                        }
# ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error)
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
# endif
                    }

                    // No need to check, because it will never overflow.
                    // utime uses time_t seconds; assign preserved seconds.
                    timestamp_spec.actime = static_cast<::std::time_t>(omit_atim_unix_timestamp.seconds);
                }

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                {
                    if(now_unix_timestamp.seconds == 0u && now_unix_timestamp.subseconds == 0u)
                    {
# ifdef UWVM_CPP_EXCEPTIONS
                        try
# endif
                        {
                            now_unix_timestamp = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime);
                        }
# ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error)
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
# endif
                    }

                    // utime uses time_t seconds; assign seconds (ns truncated).
                    timestamp_spec.modtime = static_cast<::std::time_t>(now_unix_timestamp.seconds);
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                {
                    auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};

                    if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                    {
                        if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    // utime uses time_t seconds; convert ns to seconds.
                    timestamp_spec.modtime = static_cast<::std::time_t>(mtim_seconds);
                }
                else
                {
                    if(omit_mtim_unix_timestamp.seconds == 0u && omit_mtim_unix_timestamp.subseconds == 0u)
                    {
# ifdef UWVM_CPP_EXCEPTIONS
                        try
# endif
                        {
                            auto const tmp_status{status(curr_fd_native_file)};
                            omit_atim_unix_timestamp = tmp_status.atim;
                            omit_mtim_unix_timestamp = tmp_status.mtim;
                        }
# ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error)
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
# endif
                    }

                    // No need to check, because it will never overflow.
                    // utime uses time_t seconds; assign preserved seconds.
                    timestamp_spec.modtime = static_cast<::std::time_t>(omit_mtim_unix_timestamp.seconds);
                }

                // Note that, as under DOS a file only has a single timestamp, the actime field of struct utimbuf is ignored by this function, and only modtime
                // field is used. On filesystems which support long filenames, both fields are used and both access and modification times are set.
                if(::uwvm2::imported::wasi::wasip1::func::posix::utime(fd_native_handle_pathname_cstr, ::std::addressof(timestamp_spec)) == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
# if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                        case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
# if defined(ENOTSUP)
                        case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

#elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(__MSDOS__) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
                // posix
# if defined(__linux__)
                // linux
                auto const& curr_fd_native_file{file_fd};
                auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

                if constexpr(::std::numeric_limits<::std::size_t>::max() >= ::std::numeric_limits<::std::uint_least64_t>::max())
                {
                    // 64 bit
                    // All fields in the 64-bit timespec are 64-bit fields.
                    struct timespec timestamp_spec[2];

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                    {
                        auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                        auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                    {
                        auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                        auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

#  if defined(__NR_utimensat)
                    auto const res{::fast_io::system_call<__NR_utimensat, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
                    if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
                    {
                        auto const err{static_cast<int>(-res)};
                        switch(err)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
#  else
                    if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
                    {
                        switch(errno)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
#  endif
                }
                else if constexpr(::std::numeric_limits<::std::size_t>::max() >= ::std::numeric_limits<::std::uint_least32_t>::max())
                {
                    // 32bit

#  if defined(__NR_utimensat_time64)
                    ::uwvm2::imported::wasi::wasip1::func::posix::linux_kernel_timespec64 timestamp_spec[2];

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                    {
                        timestamp_spec[0] = {{}, static_cast<::std::int_least64_t>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                    {
                        timestamp_spec[0] = {static_cast<::std::int_least64_t>(
                                                 static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u),
                                             static_cast<::std::int_least64_t>(
                                                 static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u)};
                    }
                    else
                    {
                        timestamp_spec[0] = {{}, static_cast<::std::int_least64_t>(UTIME_OMIT)};
                    }

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                    {
                        timestamp_spec[1] = {{}, static_cast<::std::int_least64_t>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                    {
                        timestamp_spec[1] = {static_cast<::std::int_least64_t>(
                                                 static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u),
                                             static_cast<::std::int_least64_t>(
                                                 static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u)};
                    }
                    else
                    {
                        timestamp_spec[1] = {{}, static_cast<::std::int_least64_t>(UTIME_OMIT)};
                    }

                    auto const res{::fast_io::system_call<__NR_utimensat_time64, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
                    if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
                    {
                        auto const err{static_cast<int>(-res)};
                        switch(err)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#   endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }

#  else

                    struct timespec timestamp_spec[2];

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                    {
                        auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                        auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                    {
                        auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                        auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

#   if defined(__NR_utimensat)
                    auto const res{::fast_io::system_call<__NR_utimensat, int>(curr_fd_native_handle, "", timestamp_spec, AT_EMPTY_PATH)};
                    if(::fast_io::linux_system_call_fails(res)) [[unlikely]]
                    {
                        auto const err{static_cast<int>(-res)};
                        switch(err)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
#    if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
#   else
                    if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
                    {
                        switch(errno)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
#    if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#    endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
#   endif
#  endif
                }
                else
                {
                    // unknown

                    struct timespec timestamp_spec[2];

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                    {
                        auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                        auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

                    if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                       ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
                    }
                    else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                            ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                    {
                        auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                        auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                        if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                        {
                            if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                            }
                        }

                        timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
                    }
                    else
                    {
                        timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
                    }

                    if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
                    {
                        switch(errno)
                        {
                            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used
                            // instead.
                            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
#  if defined(ENOTSUP)
                            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
                            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                            case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        }
                    }
                }
# elif !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 200809L
                // POSIX 2008

                auto const& curr_fd_native_file{file_fd};
                auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

                struct timespec timestamp_spec[2];

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim_now)
                {
                    timestamp_spec[0] = {{}, static_cast<long>(UTIME_NOW)};
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_atim)
                {
                    auto const atim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) / 1'000'000'000u};
                    auto const atim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim) % 1'000'000'000u};

                    if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                    {
                        if(atim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    timestamp_spec[0] = {static_cast<::std::time_t>(atim_seconds), static_cast<long>(atim_subseconds)};
                }
                else
                {
                    timestamp_spec[0] = {{}, static_cast<long>(UTIME_OMIT)};
                }

                if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim_now)
                {
                    timestamp_spec[1] = {{}, static_cast<long>(UTIME_NOW)};
                }
                else if((fstflags & ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t::filestat_set_mtim)
                {
                    auto const mtim_seconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) / 1'000'000'000u};
                    auto const mtim_subseconds{static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim) % 1'000'000'000u};

                    if constexpr(::std::numeric_limits<::std::uint_least64_t>::max() > ::std::numeric_limits<::std::time_t>::max())
                    {
                        if(mtim_seconds > ::std::numeric_limits<::std::time_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    timestamp_spec[1] = {static_cast<::std::time_t>(mtim_seconds), static_cast<long>(mtim_subseconds)};
                }
                else
                {
                    timestamp_spec[1] = {{}, static_cast<long>(UTIME_OMIT)};
                }

                if(::uwvm2::imported::wasi::wasip1::func::posix::futimens(curr_fd_native_handle, timestamp_spec) == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                        case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
#  if defined(ENOTSUP)
                        case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
#  endif
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        case ENOSYS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }
# else
                // POSIX.1-2008 formally introduced the ability to modify a file's nanosecond-precision time directly based on the file descriptor.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
# endif
#else
                // unsupported platform
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
#endif

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                // Retrieve the current directory, which is the top element of the directory stack.
                auto const& curr_dir_stack{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                if(curr_dir_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                auto const& curr_dir_stack_entry{curr_dir_stack.dir_stack.back_unchecked()};
                if(curr_dir_stack_entry.ptr == nullptr) [[unlikely]]
                {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

                auto& file_fd{curr_dir_stack_entry.ptr->dir_stack.file};

                return ::uwvm2::imported::wasi::wasip1::func::set_dir_time(file_fd, atim, mtim, fstflags);
            }
#if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            }
#endif
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_filestat_set_times(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim,
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim,
        ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags) noexcept
    {
        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
#ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"fd_filestat_set_times",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fstflags)>>>(fstflags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_filestat_set_times(",
                                fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(atim)>>>(atim),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(mtim)>>>(mtim),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fstflags)>>>(fstflags),
                                u8") (wasi-trace)\n");
#endif
        }

        return fd_filestat_set_times_base(env, fd, atim, mtim, fstflags);
    }

}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

