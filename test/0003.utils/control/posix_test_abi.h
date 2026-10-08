#pragma once
#include <fast_io.h>
// Independent filesystem oracle: these calls bind to the SDK-selected libc
// ABI directly, never to fast_io's conversion or a product filesystem alias.
// Only the known Linux native providers and Darwin SDK layouts are described;
// Windows/BSD and other libc ABIs must receive their own qualification first.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace control_test::posix_abi
{
# if defined(__APPLE__) && defined(__MACH__)
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __DARWIN_INODE64(fstat);
    [[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __DARWIN_INODE64(fstatat);
    extern "C" int control_test_native_open(char const*, int, ...) noexcept __DARWIN_ALIAS_C(open);
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("_ftruncate");
# elif defined(__GLIBC__)
#  if defined(__USE_FILE_OFFSET64)
#   if defined(__USE_TIME64_REDIRECTS) || (defined(__USE_TIME_BITS64) && defined(__TIMESIZE) && __TIMESIZE == 32)
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("__fstat64_time64");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("__fstatat64_time64");
#   else
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("fstat64");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat64");
#   endif
    extern "C" int control_test_native_open(char const*, int, ...) noexcept __asm__("open64");
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("ftruncate64");
#  else
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("fstat");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat");
    extern "C" int control_test_native_open(char const*, int, ...) noexcept __asm__("open");
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("ftruncate");
#  endif
# elif defined(__BIONIC__)
    // Bionic's public struct stat has one layout; open is not FOB64-redirected.
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("fstat");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat");
    extern "C" int control_test_native_open(char const*, int, ...) noexcept __asm__("open");
#  if defined(__USE_FILE_OFFSET64)
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("ftruncate64");
#  else
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("ftruncate");
#  endif
# elif defined(__REDIR) && defined(__DEFINED_off_t)
    // musl's public features/alltypes headers identify this native declaration
    // contract. 32-bit time64 redirects stat only; native off_t is already 64-bit.
#  if defined(_REDIR_TIME64) && _REDIR_TIME64
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("__fstat_time64");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("__fstatat_time64");
#  else
    extern "C" int control_test_native_fstat(int, struct ::stat*) noexcept __asm__("fstat");
    extern "C" int control_test_native_fstatat(int, char const*, struct ::stat*, int) noexcept __asm__("fstatat");
#  endif
    extern "C" int control_test_native_open(char const*, int, ...) noexcept __asm__("open");
    extern "C" int control_test_native_ftruncate(int, ::off_t) noexcept __asm__("ftruncate");
# else
#  error "The independent control filesystem oracle requires a qualified native libc ABI"
# endif

    inline int fstat_noexcept(int descriptor, struct ::stat* output) noexcept
    {
        // output borrows exactly one complete native SDK stat cell for this
        // synchronous call; no pointer is advanced, retained or converted.
        return control_test_native_fstat(descriptor, output);
    }
# if defined(__APPLE__) && defined(__MACH__)
    [[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
# endif
    inline int fstatat_noexcept(int directory, char const* path, struct ::stat* output, int flags) noexcept
    {
        // path borrows a complete NUL-terminated name and output one native
        // stat cell until return. The libc return/errno remain the oracle.
        return control_test_native_fstatat(directory, path, output, flags);
    }
    inline int open_noexcept(char const* path, int flags, ::mode_t permissions = 0) noexcept
    {
        // The complete pathname is borrowed only during this call. Passing a
        // mode argument does not add O_CREAT/O_TRUNC or transfer FD ownership.
        return control_test_native_open(path, flags, permissions);
    }
    // Independent public-SDK fcntl declaration. Oracle input is limited by the
    // fixture to F_GETFL/F_SETFL scalars; no production alias/conversion is used.
# if defined(__APPLE__) && defined(__MACH__)
    extern "C" int control_test_native_fcntl(int, int, ...) noexcept __DARWIN_ALIAS_C(fcntl);
# elif defined(__GLIBC__) && defined(__USE_TIME64_REDIRECTS)
    extern "C" int control_test_native_fcntl(int, int, ...) noexcept __asm__("__fcntl_time64");
# elif defined(__GLIBC__) && defined(__USE_FILE_OFFSET64)
    extern "C" int control_test_native_fcntl(int, int, ...) noexcept __asm__("fcntl64");
# else
    extern "C" int control_test_native_fcntl(int, int, ...) noexcept __asm__("fcntl");
# endif
    // Independent SDK large-file oracle: native off_t32 does not restrict
    // the existing fast_io intfpos_t64 seek contract. No production ABI alias
    // or conversion participates in these native position comparisons.
# if defined(__GLIBC__)
    using seek_offset_t = ::__off64_t;
    extern "C" seek_offset_t control_test_native_lseek(int, seek_offset_t, int) noexcept __asm__("lseek64");
# elif defined(__BIONIC__)
    using seek_offset_t = ::off64_t;
    extern "C" seek_offset_t control_test_native_lseek(int, seek_offset_t, int) noexcept __asm__("lseek64");
# elif defined(__APPLE__) && defined(__MACH__)
    using seek_offset_t = ::off_t;
    extern "C" seek_offset_t control_test_native_lseek(int, seek_offset_t, int) noexcept __asm__("_lseek");
# else
    using seek_offset_t = ::off_t;
    extern "C" seek_offset_t control_test_native_lseek(int, seek_offset_t, int) noexcept __asm__("lseek");
# endif
    inline seek_offset_t lseek_noexcept(int descriptor, seek_offset_t offset, int direction) noexcept
    { return control_test_native_lseek(descriptor, offset, direction); }
    inline int ftruncate_noexcept(int descriptor, ::off_t size) noexcept
    {
        return control_test_native_ftruncate(descriptor, size);
    }
}
#endif

// Linux transport/process helpers keep their existing product-independent
// test declarations. Filesystem oracle names above hide the legacy product
// aliases imported below; no product build includes this header.
#if defined(__linux__)
#include <uwvm2/utils/control/posix_abi.h>
#include <dirent.h>
#include <sys/prctl.h>
#include <sys/wait.h>

namespace control_test::posix_abi
{
    using namespace ::uwvm2::utils::control::posix_abi;
    extern "C" ::pid_t getppid_noexcept() noexcept __asm__("getppid");
    extern "C" ::ssize_t read_noexcept(int, void*, ::size_t) noexcept __asm__("read");
    extern "C" ::ssize_t write_noexcept(int, void const*, ::size_t) noexcept __asm__("write");
    extern "C" int dup_noexcept(int) noexcept __asm__("dup");
    extern "C" int dup2_noexcept(int, int) noexcept __asm__("dup2");
    extern "C" int pipe2_noexcept(int*, int) noexcept __asm__("pipe2");
    extern "C" int prctl_noexcept(int, ...) noexcept __asm__("prctl");
    extern "C" ::pid_t fork_noexcept() noexcept __asm__("fork");
    extern "C" int execl_noexcept(char const*, char const*, ...) noexcept __asm__("execl");
    extern "C" [[noreturn]] void _exit_noexcept(int) noexcept __asm__("_exit");
    extern "C" ::pid_t waitpid_noexcept(::pid_t, int*, int) noexcept __asm__("waitpid");
    extern "C" ::pid_t wait_noexcept(int*) noexcept __asm__("wait");
    extern "C" ::ssize_t sendmsg_noexcept(int, struct ::msghdr const*, int) noexcept __asm__("sendmsg");
    extern "C" ::DIR* opendir_noexcept(char const*) noexcept __asm__("opendir");
    extern "C" struct ::dirent* readdir_noexcept(::DIR*) noexcept __asm__("readdir");
    extern "C" int closedir_noexcept(::DIR*) noexcept __asm__("closedir");
    extern "C" int mkstemp_noexcept(char*) noexcept __asm__("mkstemp");
    extern "C" char* mkdtemp_noexcept(char*) noexcept __asm__("mkdtemp");
    extern "C" int link_noexcept(char const*, char const*) noexcept __asm__("link");
    extern "C" int symlink_noexcept(char const*, char const*) noexcept __asm__("symlink");
    extern "C" int unlink_noexcept(char const*) noexcept __asm__("unlink");
    extern "C" int rmdir_noexcept(char const*) noexcept __asm__("rmdir");
    extern "C" int posix_openpt_noexcept(int) noexcept __asm__("posix_openpt");
    extern "C" int grantpt_noexcept(int) noexcept __asm__("grantpt");
    extern "C" int unlockpt_noexcept(int) noexcept __asm__("unlockpt");
    extern "C" char* ptsname_noexcept(int) noexcept __asm__("ptsname");
    extern "C" ::pid_t setsid_noexcept() noexcept __asm__("setsid");
}
#endif
