/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#if (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)) && !defined(_WIN32)
# include <pthread.h>
# include <fcntl.h>
# if defined(__APPLE__)
#  include <mach/mach.h>
# endif
# include <signal.h>
# include <sys/mman.h>
# include <sys/resource.h>
# include <sys/types.h>
# include <unistd.h>

# if defined(__APPLE__)
#  define UWVM_RUNTIME_POSIX_SYMBOL(name) __asm__("_" #name)
# else
#  define UWVM_RUNTIME_POSIX_SYMBOL(name) __asm__(#name)
# endif

// The C declarations supplied by platform headers have no C++ exception
// specification. Distinct C-linkage aliases bind the same ABI symbols while
// telling the compiler that a POSIX call cannot unwind through guest code.
namespace uwvm2::runtime::lib::posix_abi
{
    extern "C" ::pid_t getpid_noexcept() noexcept UWVM_RUNTIME_POSIX_SYMBOL(getpid);
    extern "C" long sysconf_noexcept(int) noexcept UWVM_RUNTIME_POSIX_SYMBOL(sysconf);
    extern "C" void* mmap_noexcept(void*, ::size_t, int, int, int, ::off_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(mmap);
    extern "C" int munmap_noexcept(void*, ::size_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(munmap);
    extern "C" int mprotect_noexcept(void*, ::size_t, int) noexcept UWVM_RUNTIME_POSIX_SYMBOL(mprotect);
    extern "C" int sigaction_noexcept(int, struct ::sigaction const*, struct ::sigaction*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(sigaction);
    extern "C" int sigaltstack_noexcept(::stack_t const*, ::stack_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(sigaltstack);
    extern "C" int sigemptyset_noexcept(::sigset_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(sigemptyset);
    extern "C" int raise_noexcept(int) noexcept UWVM_RUNTIME_POSIX_SYMBOL(raise);
    extern "C" char* getenv_noexcept(char const*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(getenv);
    extern "C" [[noreturn]] void _exit_noexcept(int) noexcept UWVM_RUNTIME_POSIX_SYMBOL(_exit);

    extern "C" ::pthread_t pthread_self_noexcept() noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_self);
    extern "C" int pthread_attr_destroy_noexcept(::pthread_attr_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_attr_destroy);
    extern "C" int pthread_attr_getguardsize_noexcept(::pthread_attr_t const*, ::size_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_attr_getguardsize);
    extern "C" int pthread_key_create_noexcept(::pthread_key_t*, void (*)(void*)) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_key_create);
    extern "C" void* pthread_getspecific_noexcept(::pthread_key_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_getspecific);
    extern "C" int pthread_setspecific_noexcept(::pthread_key_t, void const*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_setspecific);
# if defined(__linux__)
    extern "C" int getrlimit_noexcept(int, struct ::rlimit*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(getrlimit);
    extern "C" int pthread_getattr_np_noexcept(::pthread_t, ::pthread_attr_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_getattr_np);
    extern "C" int pthread_attr_getstack_noexcept(::pthread_attr_t const*, void**, ::size_t*) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_attr_getstack);
# elif defined(__APPLE__)
    extern "C" ::mach_port_t pthread_mach_thread_np_noexcept(::pthread_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_mach_thread_np);
    extern "C" void* pthread_get_stackaddr_np_noexcept(::pthread_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_get_stackaddr_np);
    extern "C" ::size_t pthread_get_stacksize_np_noexcept(::pthread_t) noexcept UWVM_RUNTIME_POSIX_SYMBOL(pthread_get_stacksize_np);
# endif
}
# undef UWVM_RUNTIME_POSIX_SYMBOL
#endif
