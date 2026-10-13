/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#if (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)) && !defined(_WIN32)
# include <fcntl.h>
# include <poll.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
# if defined(__linux__)
#  include <sys/random.h>
# elif defined(__APPLE__)
#  include <libproc.h>
# endif
# if defined(__APPLE__)
#  define UWVM_CONTROL_POSIX_SYMBOL(name) __asm__("_" #name)
# else
#  define UWVM_CONTROL_POSIX_SYMBOL(name) __asm__(#name)
# endif

// C headers do not declare a C++ noexcept boundary. These declarations call
// the same libc ABI symbols while keeping host transport paths out of EH edges.
namespace uwvm2::utils::control::posix_abi
{
    extern "C" ::pid_t getpid_noexcept() noexcept UWVM_CONTROL_POSIX_SYMBOL(getpid);
    extern "C" ::uid_t getuid_noexcept() noexcept UWVM_CONTROL_POSIX_SYMBOL(getuid);
    extern "C" ::gid_t getgid_noexcept() noexcept UWVM_CONTROL_POSIX_SYMBOL(getgid);
    extern "C" int close_noexcept(int) noexcept UWVM_CONTROL_POSIX_SYMBOL(close);
    extern "C" int fcntl_noexcept(int, int, ...) noexcept UWVM_CONTROL_POSIX_SYMBOL(fcntl);
    extern "C" int isatty_noexcept(int) noexcept UWVM_CONTROL_POSIX_SYMBOL(isatty);
    extern "C" int ioctl_noexcept(int, unsigned long, ...) noexcept UWVM_CONTROL_POSIX_SYMBOL(ioctl);
# if defined(__linux__)
    extern "C" int poll_noexcept(struct ::pollfd*, ::nfds_t, int) noexcept UWVM_CONTROL_POSIX_SYMBOL(poll);
    extern "C" int socketpair_noexcept(int, int, int, int*) noexcept UWVM_CONTROL_POSIX_SYMBOL(socketpair);
    extern "C" int setsockopt_noexcept(int, int, int, void const*, ::socklen_t) noexcept UWVM_CONTROL_POSIX_SYMBOL(setsockopt);
    extern "C" ::ssize_t getrandom_noexcept(void*, ::size_t, unsigned int) noexcept UWVM_CONTROL_POSIX_SYMBOL(getrandom);
    extern "C" ::ssize_t send_noexcept(int, void const*, ::size_t, int) noexcept UWVM_CONTROL_POSIX_SYMBOL(send);
    extern "C" ::ssize_t recvmsg_noexcept(int, struct ::msghdr*, int) noexcept UWVM_CONTROL_POSIX_SYMBOL(recvmsg);
# elif defined(__APPLE__)
    extern "C" int proc_pidfdinfo_noexcept(int, int, int, void*, int) noexcept UWVM_CONTROL_POSIX_SYMBOL(proc_pidfdinfo);
# endif
}
# undef UWVM_CONTROL_POSIX_SYMBOL
#endif
