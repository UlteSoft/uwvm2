export namespace fast_io
{
    using ::fast_io::posix_tzset;
    using ::fast_io::posix_stdin_number;
    using ::fast_io::posix_stdout_number;
    using ::fast_io::posix_stderr_number;
    using ::fast_io::posix_domain_value;
    using ::fast_io::throw_posix_error;

#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)
    using ::fast_io::posix_at_entry;
    using ::fast_io::posix_at_fdcwd;
    using ::fast_io::posix_status_result;
    using ::fast_io::posix_operation_result;
    using ::fast_io::posix_read_result;
    using ::fast_io::posix_write_result;
    using ::fast_io::posix_openat_result;
    using ::fast_io::posix_getfl_result;
    using ::fast_io::posix_seek_result;
    using ::fast_io::posix_status_nothrow;
    using ::fast_io::posix_fstatat_nothrow;
    using ::fast_io::posix_openat_nothrow;
    using ::fast_io::posix_truncate_nothrow;
    using ::fast_io::posix_getfl_nothrow;
    using ::fast_io::posix_setfl_nothrow;
    using ::fast_io::posix_seek_nothrow;
    using ::fast_io::posix_read_nothrow;
    using ::fast_io::posix_write_nothrow;
    using ::fast_io::posix_fsync_nothrow;
using ::fast_io::posix_futimens_nothrow;
using ::fast_io::posix_utimensat_nothrow;
    using ::fast_io::posix_readlinkat_nothrow;
    using ::fast_io::posix_utime_options_nothrow;
    using ::fast_io::posix_close_nothrow;
#endif
#if defined(__linux__)
    using ::fast_io::posix_fdatasync_nothrow;
    using ::fast_io::posix_fadvise_nothrow;
using ::fast_io::linux_utimensat_nothrow;
using ::fast_io::linux_utimensat_time64_nothrow;
#endif

#if defined(__APPLE__) && defined(__MACH__) && defined(F_RDAHEAD) && defined(F_RDADVISE)
    using ::fast_io::posix_readahead_nothrow;
    using ::fast_io::posix_readadvise_nothrow;
#endif

#if ((!defined(_WIN32) || defined(__WINE__)) || defined(__CYGWIN__))
    namespace posix
    {
        using ::fast_io::posix::libc_ioctl;
    }

    namespace details
    {
        using ::fast_io::details::sys_mmap;
        using ::fast_io::details::sys_mprotect;
        using ::fast_io::details::sys_munmap;
        using ::fast_io::details::sys_munmap_nothrow;

    }

    using ::fast_io::posix_socklen_t;

#endif
}