// UtfUwvm2Bench.cc
//
// Per-implementation UTF-8 validation micro-benchmark for:
//   - uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>
//
// The input bytes are loaded from a file produced by UtfGen.cc so this
// benchmark can be compared fairly against other implementations in separate
// processes.
//
// Environment variables:
//   SCENARIO    : scenario name for reporting (default: "unknown")
//   DATA_FILE   : input file path (default: "utf_data.bin")
//   ITERS       : timed iterations (default: 20)
//   WARMUP_ITERS: warm-up iterations (default: 1)
//   TRIALS      : measurement trials (default: 3)
//
// Output line:
//   uwvm2_utf scenario=<...> impl=uwvm2 bytes=<...> total_ns=<...> ns_per_byte=<...> gib_per_s=<...>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <string>
#include <vector>

#include <uwvm2/utils/utf/impl.h>

namespace
{
    using clock_type = std::chrono::steady_clock;

    struct bench_result
    {
        char const* scenario{};
        char const* impl{};
        std::size_t bytes{};
        long long total_ns{};
        double ns_per_byte{};
        double gib_per_s{};
        std::uint64_t ok_count{};
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

    bench_result run_trial(char const* scenario,
                           std::byte const* data,
                           std::size_t len,
                           std::size_t iterations,
                           std::size_t warmup_iterations)
    {
        auto const* const begin = reinterpret_cast<char8_t const*>(data);
        auto const* const end = reinterpret_cast<char8_t const*>(data + len);

        std::uint64_t ok_count{};

        for(std::size_t i{}; i < warmup_iterations; ++i)
        {
            auto const r = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(begin, end);
            ok_count += static_cast<std::uint64_t>((r.err == ::uwvm2::utils::utf::utf_error_code::success) && (r.pos == end));
        }

        auto const t0{clock_type::now()};
        for(std::size_t i{}; i < iterations; ++i)
        {
            auto const r = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(begin, end);
            ok_count += static_cast<std::uint64_t>((r.err == ::uwvm2::utils::utf::utf_error_code::success) && (r.pos == end));
        }
        auto const t1{clock_type::now()};

        long long const total_ns{std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};
        double const total_bytes = static_cast<double>(len) * static_cast<double>(iterations);
        double const ns_per_byte = static_cast<double>(total_ns) / total_bytes;
        double const seconds = static_cast<double>(total_ns) * 1e-9;
        double const gib_per_s = (total_bytes / (1024.0 * 1024.0 * 1024.0)) / seconds;

        bench_result r{};
        r.scenario = scenario;
        r.impl = "uwvm2";
        r.bytes = len;
        r.total_ns = total_ns;
        r.ns_per_byte = ns_per_byte;
        r.gib_per_s = gib_per_s;
        r.ok_count = ok_count;
        return r;
    }

    bench_result run_bench(char const* scenario,
                           std::byte const* data,
                           std::size_t len,
                           std::size_t iterations,
                           std::size_t warmup_iterations,
                           std::size_t trials)
    {
        long long sum_total_ns{};
        double sum_ns_per_byte{};
        double sum_gib_per_s{};
        std::uint64_t sum_ok_count{};

        for(std::size_t t{}; t < trials; ++t)
        {
            auto const r = run_trial(scenario, data, len, iterations, warmup_iterations);
            sum_total_ns += r.total_ns;
            sum_ns_per_byte += r.ns_per_byte;
            sum_gib_per_s += r.gib_per_s;
            sum_ok_count += r.ok_count;
        }

        double const inv_trials = trials > 0u ? (1.0 / static_cast<double>(trials)) : 1.0;

        bench_result agg{};
        agg.scenario = scenario;
        agg.impl = "uwvm2";
        agg.bytes = len;
        agg.total_ns = static_cast<long long>(static_cast<double>(sum_total_ns) * inv_trials);
        agg.ns_per_byte = sum_ns_per_byte * inv_trials;
        agg.gib_per_s = sum_gib_per_s * inv_trials;
        agg.ok_count = sum_ok_count;
        return agg;
    }

    void print_bench_result(bench_result const& r)
    {
        std::printf("uwvm2_utf scenario=%s impl=%s bytes=%zu total_ns=%lld ns_per_byte=%.6f gib_per_s=%.6f ok=%llu\n",
                    r.scenario,
                    r.impl,
                    r.bytes,
                    static_cast<long long>(r.total_ns),
                    r.ns_per_byte,
                    r.gib_per_s,
                    static_cast<unsigned long long>(r.ok_count));
    }
}  // namespace

int main()
{
    std::string const scenario = read_env_string("SCENARIO", "unknown");
    std::string const data_file = read_env_string("DATA_FILE", "utf_data.bin");

    std::size_t const iterations = read_env_size("ITERS", 20u);
    std::size_t const warmup_iterations = read_env_size("WARMUP_ITERS", 1u);
    std::size_t const trials = read_env_size("TRIALS", 3u);

    auto const buffer{read_file(data_file)};

    auto const r{run_bench(scenario.c_str(),
                           buffer.data(),
                           buffer.size(),
                           iterations,
                           warmup_iterations,
                           trials)};
    print_bench_result(r);
    return 0;
}

