#include <fcntl.h>
#include <cerrno>
import fast_io;

// Requires fresh same-profile PCM/object; no vendor header injection. Module
// export and the selected SDK's signed wide overload must both be real.
static_assert(noexcept(::fast_io::posix_fadvise_nothrow(::fast_io::posix_io_observer{-1}, 0, 0, POSIX_FADV_NORMAL)));

int main()
{
    ::fast_io::posix_operation_result const result{
        ::fast_io::posix_fadvise_nothrow(::fast_io::posix_io_observer{-1}, -1, -1, POSIX_FADV_NORMAL)};
    if(result || result.error != EBADF)
    {
        ::fast_io::io::perrln("FAIL named-module Linux fadvise positive error export");
        return 1;
    }
    ::fast_io::io::println("PASS named-module Linux fadvise positive error export");
}
