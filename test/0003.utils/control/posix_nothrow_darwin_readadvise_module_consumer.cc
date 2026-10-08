#include <fcntl.h>
#include <cerrno>
import fast_io;

#if !defined(__APPLE__) || !defined(__MACH__) || !defined(F_RDAHEAD) || !defined(F_RDADVISE)
# error "API14 module consumer requires an actual full Darwin SDK"
#endif

// Compile with a freshly rebuilt fast_io PCM for the same target, SDK and
// feature macros; no forced vendor headers, old PCM or textual fast_io import.
int main()
{
    static_assert(noexcept(::fast_io::posix_readahead_nothrow(::fast_io::posix_io_observer{-1}, false)));
    static_assert(noexcept(::fast_io::posix_readadvise_nothrow(::fast_io::posix_io_observer{-1}, 0, 0)));
    auto const a{::fast_io::posix_readahead_nothrow(::fast_io::posix_io_observer{-1}, false)};
    auto const b{::fast_io::posix_readadvise_nothrow(::fast_io::posix_io_observer{-1}, -13, -1)};
    return a.error == EBADF && b.error == EBADF ? 0 : 1;
}
