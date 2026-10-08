#include <cerrno>
import fast_io;
import uwvm2.utils.control;

// Fresh named modules only, never forced-header inclusion or old PCM reuse.
// Actual compile/run must use the sole SSH Linux keeper's owned cgroup.
static_assert(noexcept(::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{-1})));

int main()
{
    namespace ctl = ::uwvm2::utils::control;
    ::fast_io::posix_getfl_result const observed{
        ::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{-1})};
    if(observed || observed.error != EBADF || ctl::console_input_sealed() ||
       ctl::seal_console_input_host_api(-1) != ctl::sealed_input_status::invalid_descriptor ||
       ctl::console_input_sealed())
    {
        ::fast_io::io::perrln("FAIL fresh named-module sealed-input FastIO flags");
        return 1;
    }
    ::fast_io::io::println("PASS fresh named-module sealed-input FastIO flags");
}
