#include <fcntl.h>
#include <limits>
#include <type_traits>
#include <fast_io.h>

#if !defined(__APPLE__) || !defined(__MACH__) || !defined(F_RDAHEAD) || !defined(F_RDADVISE)
# error "This source-contract fixture requires actual full Darwin SDK command declarations"
#endif

template<typename observer_type>
concept callable_readadvise = requires(observer_type observer)
{
    ::fast_io::posix_readadvise_nothrow(observer, ::fast_io::intfpos_t{}, int{});
};
template<typename observer_type>
concept callable_readahead = requires(observer_type observer)
{
    ::fast_io::posix_readahead_nothrow(observer, bool{});
};
constexpr bool actual_offset_fits = ::std::numeric_limits<::off_t>::is_signed &&
    ::std::numeric_limits<::off_t>::digits >= ::std::numeric_limits<::fast_io::intfpos_t>::digits;
static_assert(callable_readadvise<::fast_io::posix_io_observer> == actual_offset_fits);
static_assert(callable_readahead<::fast_io::posix_io_observer>);
static_assert(::std::is_same_v<decltype(::radvisory{}.ra_offset), ::off_t>);
static_assert(::std::is_same_v<decltype(::radvisory{}.ra_count), int>);
// Public names deliberately follow actual SDK command exposure. A strict
// _POSIX_C_SOURCE SDK without _DARWIN_C_SOURCE omits these Darwin APIs entirely.
// This compile contract never proves target linking or native behavior.
int main() {}
