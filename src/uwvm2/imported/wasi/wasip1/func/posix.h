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
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#  include <time.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  if __has_include(<poll.h>)
#   include <poll.h>
#  endif
#  if __has_include(<sys/ioctl.h>)
#   include <sys/ioctl.h>
#  endif
#  if __has_include(<sys/select.h>)
#   include <sys/select.h>
#  endif
#  if !(defined(__MSDOS__) || defined(__DJGPP__))
#   include <sys/socket.h>
#  else
#   include <utime.h>
#  endif
#  if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || (defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
#   include <sys/event.h>
#  endif
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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
#if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
    namespace posix
    {
        // https://github.com/torvalds/linux/blob/07e27ad16399afcd693be20211b0dfae63e0615f/include/uapi/linux/time_types.h#L7
        // https://github.com/qemu/qemu/blob/ab8008b231e758e03c87c1c483c03afdd9c02e19/linux-user/syscall_defs.h#L251
        // https://github.com/bminor/glibc/blob/b7e0ec907ba94b6fcc6142bbaddea995bcc3cef3/include/struct___timespec64.h#L15
        struct linux_kernel_timespec64
        {
            ::std::int_least64_t tv_sec;
            ::std::int_least64_t tv_nsec;
        };

# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
        extern int posix_fadvise(int fd, ::off_t offset, ::off_t len, int advice) noexcept __asm__("posix_fadvise");
        extern int fallocate(int fd, int mode, ::off_t offset, ::off_t len) noexcept __asm__("fallocate");
        extern int posix_fallocate(int fd, ::off_t offset, ::off_t size) noexcept __asm__("posix_fallocate");
# endif

        extern int fcntl(int fd, int cmd, ... /* arg */) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("fcntl")
# else
            __asm__("_fcntl")
# endif
                ;

        extern int ftruncate(int fd, ::off_t size) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("ftruncate")
# else
            __asm__("_ftruncate")
# endif
                ;

        extern int fstat(int fd, struct ::stat* st) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("fstat")
# else
            __asm__("_fstat")
# endif
                ;

        extern int fdatasync(int fd) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("fdatasync")
# else
            __asm__("_fdatasync")
# endif
                ;

        extern int fsync(int fd) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("fsync")
# else
            __asm__("_fsync")
# endif
                ;

        // POSIX 2008
        extern int futimens(int fd, const struct ::timespec times[2]) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("futimens")
# else
            __asm__("_futimens")
# endif
                ;

        // POSIX 2001
        extern int utimes(char const* path, const struct ::timeval times[2]) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("utimes")
# else
            __asm__("_utimes")
# endif
                ;

        // POSIX 1988 (LEGACY), only support msdos-djgpp here
# if defined(__MSDOS__) || defined(__DJGPP__)
        extern int utime(char const* path, const struct ::utimbuf* time) noexcept
#  if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("utime")
#  else
            __asm__("_utime")
#  endif
                ;
# endif

        // POSIX 2001, but msdos-djgpp not supported
# if !(defined(__MSDOS__) || defined(__DJGPP__))
        extern int getsockopt(int, int, int, void* __restrict, ::socklen_t* __restrict) noexcept
#  if !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("getsockopt")
#  else
            __asm__("_getsockopt")
#  endif
                ;
# endif

# if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || (defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
        extern int kqueue() noexcept
#  if !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("kqueue")
#  else
            __asm__("_kqueue")
#  endif
                ;

        extern int
            kevent(int kq, const struct ::kevent* changelist, int nchanges, struct ::kevent* eventlist, int nevents, const struct ::timespec* timeout) noexcept
#  if !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("kevent")
#  else
            __asm__("_kevent")
#  endif
                ;
# endif

        extern int poll(struct ::pollfd*, ::nfds_t, int) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("poll")
# else
            __asm__("_poll")
# endif
                ;

        extern int select(int, ::fd_set*, ::fd_set*, ::fd_set*, struct ::timeval*) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("select")
# else
            __asm__("_select")
# endif
                ;

        extern int sched_yield(void) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("sched_yield")
# else
            __asm__("_sched_yield")
# endif
                ;

        [[noreturn]] extern void exit(int) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("exit")
# else
            __asm__("_exit")
# endif
                ;

        extern int raise(int) noexcept
# if !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__APPLE__) || defined(__DARWIN_C_LEVEL))
            __asm__("raise")
# else
            __asm__("_raise")
# endif
                ;
    }  // namespace posix
#endif

}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

