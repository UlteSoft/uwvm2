import fast_io;

// Build from a fresh same-profile fast_io PCM/object. No header injection,
// old PCM reuse, native Darwin API or exception-dependent implementation.
static_assert(noexcept(::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{-1})));

int main()
{
    ::fast_io::posix_operation_result const data{
        ::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{-1})};
    ::fast_io::posix_operation_result const metadata{
        ::fast_io::posix_fsync_nothrow(::fast_io::posix_io_observer{-1})};
    if(data || metadata || data.error <= 0 || data.error != metadata.error)
    {
        ::fast_io::io::perrln("FAIL named-module Linux fdatasync error export");
        return 1;
    }
    ::fast_io::io::println("PASS named-module Linux fdatasync error export");
}
