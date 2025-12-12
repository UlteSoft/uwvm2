// Xxh3UtfXxhashBench.cc
//
// Micro-benchmark for upstream xxHash's XXH3_64bits on a pre-generated
// UTF-8 style buffer loaded from a file.
//
// Environment variables:
//   DATA_FILE : input path (default: "xxh3_utf_data.bin")
//   ITERS     : number of outer iterations (default: 50)
//
// Output line:
//   xxh3_utf impl=xxhash_xxh3 bytes=<...> total_ns=<...> gib_per_s=<...> checksum=<...>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

// Bring in upstream xxHash (header-only style).
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

    std::string read_env_string(char const* name, char const* default_value)
    {
        if(char const* s = std::getenv(name))
        {
            if(*s != '\0') { return std::string{s}; }
        }
        return std::string{default_value};
    }

    std::vector<std::byte> read_file(std::string const& path)
    {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if(!f)
        {
            std::perror("fopen");
            std::fprintf(stderr, "failed to open DATA_FILE for read: %s\n", path.c_str());
            std::exit(1);
        }

        if(std::fseek(f, 0, SEEK_END) != 0)
        {
            std::perror("fseek");
            std::exit(1);
        }
        long const sz = std::ftell(f);
        if(sz <= 0)
        {
            std::fprintf(stderr, "DATA_FILE appears empty: %s\n", path.c_str());
            std::exit(1);
        }
        std::rewind(f);

        std::vector<std::byte> buf(static_cast<std::size_t>(sz));
        std::size_t const n = std::fread(buf.data(), 1, buf.size(), f);
        if(n != buf.size())
        {
            std::perror("fread");
            std::fprintf(stderr, "short read from DATA_FILE: %s (%zu/%zu)\n", path.c_str(), n, buf.size());
            std::exit(1);
        }
        std::fclose(f);
        return buf;
    }

    bench_result run_bench(std::byte const* data, std::size_t len, std::size_t iterations)
    {
        using namespace std::chrono;

        std::uint64_t checksum{};
        std::size_t processed_bytes{};

        auto const t0{clock_type::now()};
        for(std::size_t i{}; i < iterations; ++i)
        {
            // Make the input depend on the iteration so the hash is not loop-invariant.
            std::size_t const offset = (i * 1315423911u) & 63u;  // [0, 63]
            std::byte const* const ptr = data + offset;
            std::size_t const cur_len = len - offset;

            checksum ^= XXH3_64bits(ptr, cur_len);
            processed_bytes += cur_len;
        }
        auto const t1{clock_type::now()};

        long long const total_ns{duration_cast<nanoseconds>(t1 - t0).count()};

        double const total_bytes{static_cast<double>(processed_bytes)};
        double const seconds{static_cast<double>(total_ns) * 1e-9};
        double const gib_per_s{(total_bytes / (1024.0 * 1024.0 * 1024.0)) / seconds};

        bench_result r{};
        r.impl = "xxhash_xxh3";
        r.bytes = processed_bytes;
        r.total_ns = total_ns;
        r.gib_per_s = gib_per_s;
        r.checksum = checksum;
        return r;
    }

    void print_bench_result(bench_result const& r)
    {
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
    std::string const data_file = read_env_string("DATA_FILE", "xxh3_utf_data.bin");
    std::size_t const iterations = read_env_size("ITERS", 50u);

    auto const buffer{read_file(data_file)};

    auto const r{run_bench(buffer.data(), buffer.size(), iterations)};
    print_bench_result(r);

    return 0;
}
