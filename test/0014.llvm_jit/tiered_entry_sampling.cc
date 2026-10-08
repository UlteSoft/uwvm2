#include <uwvm2/runtime/lib/uwvm_runtime_tiered_entry_sampling.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace
{
    unsigned checks{};
    void check(bool value)
    {
        ++checks;
        if(!value) { std::fprintf(stderr, "FAIL tiered sampler check %u\n", checks); std::abort(); }
    }
}

// Inspect the actual optimized production helper for one existing TLS-state
// access and arithmetic only. There must be no call, lock, or hidden helper.
extern "C" bool probe_stride4(std::uint_least32_t& state, std::size_t function) noexcept
{ return uwvm2::runtime::lib::details::tiered_entry_sample_advance(state, function, 4u); }
extern "C" bool probe_stride16(std::uint_least32_t& state, std::size_t function) noexcept
{ return uwvm2::runtime::lib::details::tiered_entry_sample_advance(state, function, 16u); }

int main()
{
    using uwvm2::runtime::lib::details::tiered_entry_sample_advance;
    // The exact previously starving sequence: initial entry fn2, then fn1/fn0.
    // The old low-bit counter never samples either callee in this sequence.
    for(auto seed : {0u, 1u, 0xfffffff0u, 0xffffffffu})
    {
        for(auto stride : {4u, 8u, 16u})
        {
            auto state{seed};
            static_cast<void>(tiered_entry_sample_advance(state, 2u, stride));
            std::array<unsigned, 2u> count{};
            for(unsigned repeat{}; repeat != 131072u; ++repeat)
            {
                count[0] += tiered_entry_sample_advance(state, 1u, stride);
                count[1] += tiered_entry_sample_advance(state, 0u, stride);
            }
            for(auto samples : count)
            {
                auto const expected{131072u / stride};
                check(samples > expected * 95u / 100u && samples < expected * 105u / 100u);
            }
        }
    }
    // Cover short/long regular call graphs and the uint32 wrap. Each individual
    // function must receive the intended sample rate, not just the aggregate.
    for(auto stride : {4u, 8u, 16u})
    {
        for(auto period : {1u, 2u, 3u, 4u, 7u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, 1024u})
        {
            std::array<unsigned, 1024u> counts{};
            std::uint_least32_t state{0xfffffff0u};
            for(unsigned round{}; round != 16384u; ++round)
            {
                for(unsigned pos{}; pos != period; ++pos)
                { counts[pos] += tiered_entry_sample_advance(state, (pos * 17u + 11u) % period, stride); }
            }
            for(unsigned pos{}; pos != period; ++pos)
            {
                auto const expected{16384u / stride};
                check(counts[pos] > expected * 90u / 100u && counts[pos] < expected * 110u / 100u);
            }
        }
    }
    std::uint_least32_t unchanged{42u};
    check(tiered_entry_sample_advance(unchanged, 0u, 1u));
    check(unchanged == 42u);
    std::printf("PASS tiered entry sampling: %u checks\n", checks);
}
