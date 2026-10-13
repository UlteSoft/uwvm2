// Independent integer oracle for the benchmark's immutable read region.
// Read and write regions are disjoint; native load forwarding cannot replace
// these reads with the loop's preceding store value.
#pragma once
#include <cstdint>
#include <string_view>

inline std::uint32_t memory_performance_byte(std::uint32_t index)
{
    auto value{index + 0x9e3779b9u};
    value = (value ^ (value >> 16u)) * 0x7feb352du;
    value = (value ^ (value >> 15u)) * 0x846ca68bu;
    return (value ^ (value >> 16u)) & 255u;
}

inline std::uint32_t memory_performance_checksum(char const* path, std::uint32_t count)
{
    std::string_view name{path};
    auto const separator{name.find_last_of("/\\")};
    if(separator != std::string_view::npos) { name.remove_prefix(separator + 1u); }
    std::uint32_t expected{};
    // XOR of 0..count-1; benchmark counts are positive.
    switch(count & 3u)
    {
        case 1: expected = count - 1u; break;
        case 2: expected = 1u; break;
        case 3: expected = count; break;
    }
    if(!name.starts_with("load-")) { return expected; }
    bool const unaligned{name.find("unaligned") != std::string_view::npos};
    bool const vector{name.starts_with("load-simd-")};
    constexpr std::uint32_t period{16384};
    auto const remainder{count % period};
    bool const full_period{(count / period) % 2u != 0u};
    auto const limit{full_period ? period : remainder};
    for(std::uint32_t index{}; index != limit; ++index)
    {
        if(full_period && index < remainder) { continue; } // two copies cancel
        auto const offset{index * 16u + (unaligned ? 13u : 0u)};
        for(unsigned lane{}; lane != (vector ? 4u : 1u); ++lane)
        {
            for(unsigned byte{}; byte != 4u; ++byte)
            { expected ^= memory_performance_byte(offset + lane * 4u + byte) << (byte * 8u); }
        }
    }
    return expected;
}
