// A correct fenv does not force each C++ expression to Wasm binary32/64
// precision. Probe excess precision separately; volatile stores alone cannot
// undo an earlier extended-precision rounding to an exact destination midpoint.
// Diagnostic for a pre-existing native arithmetic limitation, NOT a test that the environment guard can fix.
// x87/68881 extended intermediates can double-round even with FE_TONEAREST and all FP boundaries protected.
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cfenv>
template<typename Float>
[[nodiscard]] __attribute__((noinline)) std::uint64_t add(std::uint64_t left, std::uint64_t right)
{
    volatile Float lhs{std::bit_cast<Float>(left)}, rhs{std::bit_cast<Float>(right)};
    Float result{lhs + rhs};
    return std::bit_cast<std::uint64_t>(result);
}
int main()
{
    if(std::fesetround(FE_TONEAREST) != 0) { return 2; }
    auto result{add<double>(0x3ff0000000000000ull, 0x3ca0000000000001ull)};
    std::printf("double=%llx expected=3ff0000000000001\n", static_cast<unsigned long long>(result));
#ifdef __STDCPP_FLOAT64_T__
    auto result64{add<_Float64>(0x3ff0000000000000ull, 0x3ca0000000000001ull)};
    std::printf("_Float64=%llx expected=3ff0000000000001\n", static_cast<unsigned long long>(result64));
    if(result64 != 0x3ff0000000000001ull) { return 1; }
#endif
    return result == 0x3ff0000000000001ull ? 0 : 1;
}
