// oracle-values.inc is generated from ieee_oracle.py using integers/rationals.
#include <fast_io.h>
#include <cstddef>
#include <cstdint>

struct witness { std::uint64_t input, value[4], mask[4]; };
#include "oracle-values.inc"

#define DECLARE(prefix) \
    extern "C" void prefix##_ceil(void const*, void*); \
    extern "C" void prefix##_floor(void const*, void*); \
    extern "C" void prefix##_trunc(void const*, void*); \
    extern "C" void prefix##_nearest(void const*, void*);
DECLARE(native32) DECLARE(native64) DECLARE(bits32) DECLARE(bits64)
#undef DECLARE
using operation = void (*)(void const*, void*);
#define ROW(prefix) {prefix##_ceil, prefix##_floor, prefix##_trunc, prefix##_nearest}
operation const functions[2][2][4]{{ROW(native32), ROW(native64)}, {ROW(bits32), ROW(bits64)}};
#undef ROW

[[nodiscard]] unsigned read_fcsr() noexcept
{
    unsigned result;
    asm volatile("cfc1 %0, $31" : "=r"(result));
    return result;
}
void write_fcsr(unsigned value) noexcept
{
    asm volatile("ctc1 %0, $31\n\tnop\n\tnop" :: "r"(value) : "memory");
}

int main()
{
    auto const saved{read_fcsr()};
    std::uint64_t checks{};
    for(unsigned mode{}; mode != 4u; ++mode)
    {
        // Mask exceptions and disable flush-to-zero, preserve the native NaN convention.
        write_fcsr((saved & ~((1u << 24u) | 0x1ffffu)) | mode);
        for(unsigned variant{}; variant != 2u; ++variant)
        for(unsigned width{}; width != 2u; ++width)
        {
            auto samples{width == 0u ? samples32 : samples64};
            auto count{width == 0u ? sizeof(samples32) / sizeof(witness) : sizeof(samples64) / sizeof(witness)};
            auto bytes{width == 0u ? 4u : 8u};
            auto lanes{16u / bytes};
            for(std::size_t i{}; i != count; ++i)
            {
                unsigned char input[16], result[16];
                for(unsigned lane{}; lane != lanes; ++lane)
                for(unsigned byte{}; byte != bytes; ++byte)
                { input[lane * bytes + byte] = static_cast<unsigned char>(samples[(i + lane) % count].input >> (8u * byte)); }
                for(unsigned op{}; op != 4u; ++op)
                {
                    functions[variant][width][op](input, result);
                    for(unsigned lane{}; lane != lanes; ++lane)
                    {
                        std::uint64_t actual{};
                        for(unsigned byte{}; byte != bytes; ++byte)
                        { actual |= static_cast<std::uint64_t>(result[lane * bytes + byte]) << (8u * byte); }
                        auto const& sample{samples[(i + lane) % count]};
                        ++checks;
                        if((actual & sample.mask[op]) != sample.value[op])
                        {
                            write_fcsr(saved);
                            fast_io::println(fast_io::err(), "FAIL variant=", variant, " width=", bytes * 8u,
                                " mode=", mode, " op=", op, " input=", fast_io::mnp::hex(sample.input),
                                " actual=", fast_io::mnp::hex(actual), " expected=", fast_io::mnp::hex(sample.value[op]));
                            return 1;
                        }
                    }
                }
            }
        }
        if((read_fcsr() & 3u) != mode) { write_fcsr(saved); return 2; }
    }
    write_fcsr(saved);
    fast_io::println("PASS checks=", checks, " modes=4 production_variants=2 exact_or_legal_NaN");
}
