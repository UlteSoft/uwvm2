#include <uwvm2/runtime/lib/uwvm_runtime_checked_size.h>
#include <cstdint>
#include <cstdio>

template <typename Size>
constexpr bool check()
{
    using uwvm2::runtime::lib::details::try_add_runtime_byte_extents;
    constexpr Size maximum{std::numeric_limits<Size>::max()};
    Size result{123};
    if(try_add_runtime_byte_extents(Size{16}, Size{15}, Size(maximum - 11), result) || result != 123) { return false; }
    if(try_add_runtime_byte_extents(Size(maximum - 7), Size{15}, Size{0}, result) || result != 123) { return false; }
    if(!try_add_runtime_byte_extents(Size{16}, Size{15}, Size{32}, result) || result != 63) { return false; }
    if(!try_add_runtime_byte_extents(Size(maximum - 31), Size{15}, Size{16}, result) || result != maximum) { return false; }
    if(!try_add_runtime_byte_extents(Size{0}, Size{0}, Size{0}, result) || result != 0) { return false; }
    // The output may alias the input extent, as in the production frame builder.
    result = 16;
    if(!try_add_runtime_byte_extents(result, Size{15}, Size{32}, result) || result != 63) { return false; }
    return true;
}
static_assert(check<std::uint32_t>());
static_assert(check<std::uint64_t>());
int main()
{
    if(!check<std::uint32_t>() || !check<std::uint64_t>()) { return 1; }
    std::puts("PASS checked frame extents (32-bit and 64-bit)");
}
