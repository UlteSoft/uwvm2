// The TLS interpreter calls square_root on every target, including targets
// without x87/68881. Check the production helper directly in IEEE bit form.
#include <uwvm2/runtime/compiler/shared/strict_float.h>
#include <bit>
#include <cstdint>

namespace fp = uwvm2::runtime::compiler::shared::strict_float;

template <typename Float, typename UInt>
bool check(UInt input, UInt expected)
{
    UInt volatile stored = input;
    UInt const raw = stored;
    return std::bit_cast<UInt>(fp::square_root(std::bit_cast<Float>(raw))) == expected;
}

int main()
{
    bool good = check<float>(0u, 0u) && check<float>(0x80000000u, 0x80000000u) &&
        check<float>(0x40800000u, 0x40000000u) && check<float>(0x40000000u, 0x3fb504f3u) &&
        check<float>(0x7f800000u, 0x7f800000u) && check<float>(0x00000001u, 0x1a3504f3u) &&
        check<double>(std::uint64_t{0}, std::uint64_t{0}) &&
        check<double>(std::uint64_t{0x8000000000000000ull}, std::uint64_t{0x8000000000000000ull}) &&
        check<double>(std::uint64_t{0x4010000000000000ull}, std::uint64_t{0x4000000000000000ull}) &&
        check<double>(std::uint64_t{0x4000000000000000ull}, std::uint64_t{0x3ff6a09e667f3bcdull}) &&
        check<double>(std::uint64_t{0x7ff0000000000000ull}, std::uint64_t{0x7ff0000000000000ull}) &&
        check<double>(std::uint64_t{1}, std::uint64_t{0x1e60000000000000ull});
    float volatile negative_f = -1.0f;
    double volatile negative_d = -1.0;
    auto const f = std::bit_cast<std::uint32_t>(fp::square_root(static_cast<float>(negative_f)));
    auto const d = std::bit_cast<std::uint64_t>(fp::square_root(static_cast<double>(negative_d)));
    good = good && (f & 0x7fc00000u) == 0x7fc00000u &&
        (d & 0x7ff8000000000000ull) == 0x7ff8000000000000ull;
    return !good;
}
