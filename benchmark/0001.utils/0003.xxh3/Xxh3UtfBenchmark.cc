// Xxh3UtfBenchmark.cc
//
// Micro-benchmark comparing:
//   - uwvm2::utils::hash::xxh3_64bits
//   - upstream xxHash's XXH3_64bits
//
// on the same UTF-8 style text buffer.
//
// The upstream xxHash header is expected to come from a cloned
// repository (see the Lua driver in this directory) and be available
// as "xxhash.h" on the include path.
//
// Usage (from project root, via Lua driver):
//
//   lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
//   # or
//   xmake lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
//
// Environment variables consumed by this C++ benchmark:
//
//   BYTES   : total size of the UTF-8 buffer to hash (in bytes)
//             default: 16 * 1024 * 1024
//   ITERS   : number of outer iterations (hashes of the full buffer)
//             default: 50
//
// Machine-readable output lines look like:
//
//   xxh3_utf impl=<uwvm2_xxh3|xxhash_xxh3> bytes=<...> total_ns=<...> gib_per_s=<...> checksum=<...>
//
// They are parsed by the Lua driver to compare throughput.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>

#include <uwvm2/utils/hash/xxh3.h>

// Bring in upstream xxHash (header-only style).
// The Lua driver passes -DXXH_INLINE_ALL and -DXXH_STATIC_LINKING_ONLY,
// but we also guard here so the file can be built manually if needed.
#ifndef XXH_INLINE_ALL
# define XXH_INLINE_ALL
#endif
#ifndef XXH_STATIC_LINKING_ONLY
# define XXH_STATIC_LINKING_ONLY
#endif
#include "xxhash.h"

namespace
{
    using clock_type = std::chrono::steady_clock;

    struct bench_result
    {
        char const* impl{};
        std::size_t bytes{};
        long long total_ns{};
        double gib_per_s{};
        std::uint64_t checksum{};
    };

    // Read an environment variable as size_t; fall back to a default if missing or invalid.
    std::size_t read_env_size(char const* name, std::size_t default_value)
    {
        if(char const* s = std::getenv(name))
        {
            char* end{};
            unsigned long long v = std::strtoull(s, &end, 10);
            if(end && *end == '\0' && v > 0) { return static_cast<std::size_t>(v); }
        }
        return default_value;
    }

    // Create a UTF-8 style buffer by repeating a fixed base text that mixes
    // ASCII and multi-byte UTF-8 code points, until the requested size.
    std::vector<std::byte> make_utf8_buffer(std::size_t total_bytes)
    {
        // Mix of ASCII, Latin, CJK, emoji-like sequences, etc.
        std::u8string base_text =
            u8"Hello, 世界 — uwvm2 xxh3 UTF-8 benchmark.\n"
            u8"这里是一些中文字符，用来测试多字节 UTF-8 序列。\n"
            u8"また、いくつかの日本語テキストも含めます。\n"
            u8"Some ASCII-only lines as well to match typical WASI/UTF traffic.\n"
            u8"🚀✨ Unicode symbols and emoji-like code points.\n";

        std::vector<std::byte> buf;
        buf.reserve(total_bytes);

        while(buf.size() < total_bytes)
        {
            for(char8_t ch: base_text)
            {
                if(buf.size() >= total_bytes) { break; }
                buf.push_back(static_cast<std::byte>(ch));
            }
        }

        return buf;
    }

    bench_result run_bench(char const* impl_name, std::byte const* data, std::size_t len, std::size_t iterations, bool use_uwvm2_xxh3)
    {
        using namespace std::chrono;

        std::uint64_t checksum{};

        auto const t0{clock_type::now()};
        for(std::size_t i{}; i < iterations; ++i)
        {
            if(use_uwvm2_xxh3) { checksum ^= ::uwvm2::utils::hash::xxh3_64bits(data, len); }
            else
            {
                checksum ^= XXH3_64bits(data, len);
            }
        }
        auto const t1{clock_type::now()};

        long long const total_ns{duration_cast<nanoseconds>(t1 - t0).count()};

        double const total_bytes{static_cast<double>(len) * static_cast<double>(iterations)};
        double const seconds{static_cast<double>(total_ns) * 1e-9};
        double const gib_per_s{(total_bytes / (1024.0 * 1024.0 * 1024.0)) / seconds};

        bench_result r{};
        r.impl = impl_name;
        r.bytes = len * iterations;
        r.total_ns = total_ns;
        r.gib_per_s = gib_per_s;
        r.checksum = checksum;
        return r;
    }

    void print_bench_result(bench_result const& r)
    {
        // Single machine-readable line for the Lua driver.
        std::printf("xxh3_utf impl=%s bytes=%zu total_ns=%lld gib_per_s=%.6f checksum=%llu\n",
                    r.impl,
                    r.bytes,
                    static_cast<long long>(r.total_ns),
                    r.gib_per_s,
                    static_cast<unsigned long long>(r.checksum));
    }
}  // namespace

int main()
{
    // Defaults roughly tuned for a few hundred milliseconds per run on a modern CPU.
    std::size_t const total_bytes = read_env_size("BYTES", 16u * 1024u * 1024u);
    std::size_t const iterations = read_env_size("ITERS", 50u);

    std::puts("uwvm2 xxh3 vs upstream XXH3 UTF-8 buffer benchmark");
    std::printf("  total_bytes = %zu\n", total_bytes);
    std::printf("  iterations  = %zu\n", iterations);

    auto const buffer{make_utf8_buffer(total_bytes)};
    auto const* const data{buffer.data()};

    auto const r_uwvm2{run_bench("uwvm2_xxh3", data, buffer.size(), iterations, true)};
    print_bench_result(r_uwvm2);

    auto const r_xxhash{run_bench("xxhash_xxh3", data, buffer.size(), iterations, false)};
    print_bench_result(r_xxhash);

    return 0;
}

