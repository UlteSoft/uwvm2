#pragma once

#if defined(__linux__) || defined(__APPLE__)
# include <cstddef>
# include <pthread.h>
# include <sched.h>
# include <signal.h>
# include <sys/mman.h>
# include <sys/resource.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

# if defined(__APPLE__)
#  define UWVM2TEST_POSIX_SYMBOL(name) __asm__("_" #name)
# else
#  define UWVM2TEST_POSIX_SYMBOL(name) __asm__(#name)
# endif

// POSIX headers typically leave the C++ function type potentially throwing.
// Keep test harness calls on their exact C symbols, with a truthful noexcept
// type, so an unwind through a test helper cannot be attributed to libc.
namespace uwvm2test::posix_abi
{
    extern "C" int pipe_noexcept(int*) noexcept UWVM2TEST_POSIX_SYMBOL(pipe);
    extern "C" ::pid_t fork_noexcept() noexcept UWVM2TEST_POSIX_SYMBOL(fork);
    extern "C" int close_noexcept(int) noexcept UWVM2TEST_POSIX_SYMBOL(close);
    extern "C" int dup2_noexcept(int, int) noexcept UWVM2TEST_POSIX_SYMBOL(dup2);
    extern "C" [[noreturn]] void _exit_noexcept(int) noexcept UWVM2TEST_POSIX_SYMBOL(_exit);
    extern "C" ::ssize_t read_noexcept(int, void*, ::size_t) noexcept UWVM2TEST_POSIX_SYMBOL(read);
    extern "C" ::ssize_t write_noexcept(int, void const*, ::size_t) noexcept UWVM2TEST_POSIX_SYMBOL(write);
    extern "C" ::pid_t waitpid_noexcept(::pid_t, int*, int) noexcept UWVM2TEST_POSIX_SYMBOL(waitpid);
    extern "C" int sigemptyset_noexcept(::sigset_t*) noexcept UWVM2TEST_POSIX_SYMBOL(sigemptyset);
    extern "C" int sigaction_noexcept(int, struct ::sigaction const*, struct ::sigaction*) noexcept UWVM2TEST_POSIX_SYMBOL(sigaction);
    extern "C" int sigaltstack_noexcept(::stack_t const*, ::stack_t*) noexcept UWVM2TEST_POSIX_SYMBOL(sigaltstack);
    extern "C" int raise_noexcept(int) noexcept UWVM2TEST_POSIX_SYMBOL(raise);
    extern "C" long sysconf_noexcept(int) noexcept UWVM2TEST_POSIX_SYMBOL(sysconf);
    extern "C" void* mmap_noexcept(void*, ::size_t, int, int, int, ::off_t) noexcept UWVM2TEST_POSIX_SYMBOL(mmap);
    extern "C" int mprotect_noexcept(void*, ::size_t, int) noexcept UWVM2TEST_POSIX_SYMBOL(mprotect);
    extern "C" int munmap_noexcept(void*, ::size_t) noexcept UWVM2TEST_POSIX_SYMBOL(munmap);
    extern "C" int mincore_noexcept(void*, ::size_t, void*) noexcept UWVM2TEST_POSIX_SYMBOL(mincore);
    extern "C" int sched_yield_noexcept() noexcept UWVM2TEST_POSIX_SYMBOL(sched_yield);
    extern "C" int getrlimit_noexcept(int, struct ::rlimit*) noexcept UWVM2TEST_POSIX_SYMBOL(getrlimit);
    extern "C" int setrlimit_noexcept(int, struct ::rlimit const*) noexcept UWVM2TEST_POSIX_SYMBOL(setrlimit);
    extern "C" int pthread_key_create_noexcept(::pthread_key_t*, void (*)(void*)) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_key_create);
    extern "C" int pthread_key_delete_noexcept(::pthread_key_t) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_key_delete);
    extern "C" int pthread_setspecific_noexcept(::pthread_key_t, void const*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_setspecific);
    extern "C" int pthread_attr_init_noexcept(::pthread_attr_t*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_attr_init);
    extern "C" int pthread_attr_destroy_noexcept(::pthread_attr_t*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_attr_destroy);
    extern "C" int pthread_attr_setstack_noexcept(::pthread_attr_t*, void*, ::size_t) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_attr_setstack);
    extern "C" int pthread_attr_setstacksize_noexcept(::pthread_attr_t*, ::size_t) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_attr_setstacksize);
    extern "C" int pthread_create_noexcept(::pthread_t*, ::pthread_attr_t const*, void* (*)(void*), void*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_create);
    extern "C" int pthread_join_noexcept(::pthread_t, void**) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_join);
    extern "C" ::pthread_t pthread_self_noexcept() noexcept UWVM2TEST_POSIX_SYMBOL(pthread_self);
# if defined(__linux__)
    extern "C" int pthread_getattr_np_noexcept(::pthread_t, ::pthread_attr_t*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_getattr_np);
    extern "C" int pthread_attr_getstack_noexcept(::pthread_attr_t const*, void**, ::size_t*) noexcept UWVM2TEST_POSIX_SYMBOL(pthread_attr_getstack);
# endif
}
# undef UWVM2TEST_POSIX_SYMBOL
#endif
