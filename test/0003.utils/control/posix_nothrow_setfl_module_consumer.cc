import fast_io;

// This TU must be compiled against a freshly produced fast_io PCM, never an
// old module artifact or a forced header inclusion. Execute only in the sole
// SSH Linux keeper's owned cgroup. The companion SDK unit is the flag oracle.
static_assert(noexcept(::fast_io::posix_setfl_nothrow(::fast_io::posix_io_observer{-1}, 0)));

int main()
{
    ::fast_io::posix_operation_result const changed{
        ::fast_io::posix_setfl_nothrow(::fast_io::posix_io_observer{-1}, 0)};
    ::fast_io::posix_getfl_result const observed{
        ::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{-1})};
    if(changed || observed || changed.error <= 0 || changed.error != observed.error)
    {
        ::fast_io::io::perrln("FAIL named-module POSIX F_SETFL/F_GETFL error exports");
        return 1;
    }
    ::fast_io::io::println("PASS named-module POSIX F_SETFL/F_GETFL error exports");
}
