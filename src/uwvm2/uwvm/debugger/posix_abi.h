/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
# include <stddef.h>
# include <fcntl.h>
# include <poll.h>
# include <signal.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
# if defined(__APPLE__) && defined(__MACH__)
#  include <pthread.h>
#  include <mach/mach.h>
# endif

# if defined(__APPLE__) && defined(__MACH__)
#  define UWVM_DEBUGGER_POSIX_SYMBOL(name) __asm__("_" #name)
# else
#  define UWVM_DEBUGGER_POSIX_SYMBOL(name) __asm__(#name)
# endif

// The system headers expose C functions without C++ noexcept. These distinct
// C-linkage names bind to the exact libc symbols and carry their non-throwing
// ABI into debugger/control code, including signal and socket hot paths.
namespace uwvm2::uwvm::debugger::posix_abi
{
    extern "C" int fcntl_noexcept(int, int, ...) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(fcntl);
    extern "C" ::pid_t getpid_noexcept() noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getpid);
    extern "C" ::pid_t getppid_noexcept() noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getppid);
    extern "C" ::uid_t geteuid_noexcept() noexcept UWVM_DEBUGGER_POSIX_SYMBOL(geteuid);
    extern "C" ::gid_t getegid_noexcept() noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getegid);
    extern "C" int getpeername_noexcept(int, ::sockaddr*, ::socklen_t*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getpeername);
    extern "C" int getsockname_noexcept(int, ::sockaddr*, ::socklen_t*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getsockname);
    extern "C" int getsockopt_noexcept(int, int, int, void*, ::socklen_t*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(getsockopt);
    extern "C" int kill_noexcept(::pid_t, int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(kill);
    extern "C" int poll_noexcept(::pollfd*, ::nfds_t, int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(poll);
    extern "C" ::ssize_t recvmsg_noexcept(int, ::msghdr*, int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(recvmsg);
    extern "C" ::ssize_t send_noexcept(int, void const*, ::size_t, int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(send);
    extern "C" int setsockopt_noexcept(int, int, int, void const*, ::socklen_t) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(setsockopt);
    extern "C" int shutdown_noexcept(int, int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(shutdown);
    extern "C" [[noreturn]] void _exit_noexcept(int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(_exit);
# if defined(__linux__)
    extern "C" int raise_noexcept(int) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(raise);
    extern "C" int sigaction_noexcept(int, struct ::sigaction const*, struct ::sigaction*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(sigaction);
    extern "C" int sigemptyset_noexcept(::sigset_t*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(sigemptyset);
    extern "C" long syscall_noexcept(long, ...) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(syscall);
# else
    extern "C" int pthread_create_noexcept(::pthread_t*, ::pthread_attr_t const*, void* (*)(void*), void*) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(pthread_create);
    extern "C" int pthread_join_noexcept(::pthread_t, void**) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(pthread_join);
    extern "C" ::pthread_t pthread_self_noexcept() noexcept UWVM_DEBUGGER_POSIX_SYMBOL(pthread_self);
    extern "C" ::mach_port_t pthread_mach_thread_np_noexcept(::pthread_t) noexcept UWVM_DEBUGGER_POSIX_SYMBOL(pthread_mach_thread_np);
# endif
}
# undef UWVM_DEBUGGER_POSIX_SYMBOL
#endif
