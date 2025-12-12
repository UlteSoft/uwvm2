// UtfSimdUtf.cc
//
// Micro-benchmark comparing:
//   - uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked
//   - simdutf::validate_utf8
//
// The benchmark generates several representative UTF-8 workloads, runs both
// implementations over the same buffers, and reports timing in a simple,
// machine-readable format:
//
//   uwvm2_utf scenario=<...> impl=<uwvm2|simdutf> bytes=<...> total_ns=<...> \
//             ns_per_byte=<...> gib_per_s=<...>
//
// A Lua driver script in the same directory is responsible for:
//   - cloning / locating a simdutf checkout;
//   - compiling this file with appropriate include paths and SIMD flags;
//   - running the benchmark and summarizing results.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <string>

#include <uwvm2/utils/utf/impl.h>

// simdutf is provided by an external checkout; the Lua driver adds the
// necessary -I path (either simdutf/singleheader or simdutf/include).
#include <simdutf.h>

using namespace std::literals;

enum class scenario_kind
{
    ascii_only,
    latin_mixed,
    emoji_mixed
};

struct scenario_config
{
    char const* name{};
    scenario_kind kind{};
};

struct bench_result
{
    char const* scenario{};
    char const* impl{};
    std::size_t bytes{};
    long long total_ns{};
    double ns_per_byte{};
    double gib_per_s{};
};

// Read an environment variable as size_t; fall back to a default if missing or invalid.
static std::size_t read_env_size(char const* name, std::size_t default_value)
{
    if(char const* s = std::getenv(name))
    {
        char* end{};
        unsigned long long v = std::strtoull(s, &end, 10);
        if(end && *end == '\0' && v > 0) { return static_cast<std::size_t>(v); }
    }
    return default_value;
}

// Predefined valid UTF-8 chunks ------------------------------------------------

static std::u8string make_ascii_chunk()
{
    // Pure ASCII workload: typical WASI paths, HTTP headers, etc.
    return u8"Hello, world! 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ "
           u8"abcdefghijklmnopqrstuvwxyz /tmp/www/index.html "
           u8"-- uwvm2 utf8 ascii-only benchmark payload --\n";
}

static std::u8string make_latin_chunk()
{
    // Mixed Latin-1–style accented characters, still valid UTF-8.
    return u8"caf\u00E9, cr\u00E8me br\u00FBl\u00E9e, Jahrtausendwende, "
           u8"\u00DCber, Stra\u00DFe, Espa\u00F1a, Portugu\u00EAs, "
           u8"uwvm2 utf8 latin-mixed payload.\n";
}

static std::u8string make_emoji_chunk()
{
    // Include some 4-byte code points (Emoji, CJK, etc.).
    return u8"Emoji: \U0001F600 \U0001F602 \U0001F603 \U0001F604 "
           u8"Rocket: \U0001F680 Explosion: \U0001F4A5 "
           u8"CJK: \u4E2D\u6587\u6D4B\u8BD5 UTF-8 benchmark uwvm2.\n";
}

static std::vector<char8_t> make_buffer(scenario_config cfg, std::size_t target_bytes)
{
    std::u8string chunk;
    switch(cfg.kind)
    {
        case scenario_kind::ascii_only:
        {
            chunk = make_ascii_chunk();
            break;
        }
        case scenario_kind::latin_mixed:
        {
            chunk = make_latin_chunk();
            break;
        }
        case scenario_kind::emoji_mixed:
        {
            chunk = make_emoji_chunk();
            break;
        }
        default:
        {
            std::fprintf(stderr, "make_buffer: unknown scenario_kind\n");
            std::abort();
        }
    }

    if(chunk.empty())
    {
        std::fprintf(stderr, "make_buffer: chunk is unexpectedly empty\n");
        std::abort();
    }

    std::size_t const chunk_size{chunk.size()};

    // Repeat full chunks up to (or slightly exceeding) target_bytes.
    // We never cut inside a chunk so the result stays valid UTF-8.
    std::size_t repeats = target_bytes / chunk_size;
    if(repeats == 0) { repeats = 1; }

    std::vector<char8_t> buffer;
    buffer.reserve(chunk_size * repeats);

    for(std::size_t i{}; i < repeats; ++i) { buffer.insert(buffer.end(), chunk.begin(), chunk.end()); }

    return buffer;
}

static bench_result run_benchmark(scenario_config const& cfg,
                                  char const* impl_name,
                                  bool use_uwvm2,
                                  std::vector<char8_t> const& buffer,
                                  std::size_t iterations,
                                  std::size_t warmup_iterations)
{
    auto const* const begin{buffer.data()};
    auto const* const end{buffer.data() + buffer.size()};

    bool all_ok{true};

    // Warm-up phase (not timed): run the implementation a configurable number
    // of iterations to mitigate cold-cache / cold-branch effects before the
    // measured loop.
    for(std::size_t i{}; i < warmup_iterations; ++i)
    {
        if(use_uwvm2)
        {
            auto const r = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(begin, end);
            all_ok = all_ok && (r.err == ::uwvm2::utils::utf::utf_error_code::success) && (r.pos == end);
        }
        else
        {
            bool const ok = ::simdutf::validate_utf8(reinterpret_cast<char const*>(begin), buffer.size());
            all_ok = all_ok && ok;
        }
    }

    auto const t0{std::chrono::steady_clock::now()};

    for(std::size_t i{}; i < iterations; ++i)
    {
        if(use_uwvm2)
        {
            auto const r = ::uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>(begin, end);
            all_ok = all_ok && (r.err == ::uwvm2::utils::utf::utf_error_code::success) && (r.pos == end);
        }
        else
        {
            bool const ok = ::simdutf::validate_utf8(reinterpret_cast<char const*>(begin), buffer.size());
            all_ok = all_ok && ok;
        }
    }

    auto const t1{std::chrono::steady_clock::now()};

    long long const total_ns{std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};

    double const total_bytes{static_cast<double>(buffer.size()) * static_cast<double>(iterations)};
    double const ns_per_byte{static_cast<double>(total_ns) / total_bytes};

    double const seconds{static_cast<double>(total_ns) * 1e-9};
    double const gib_per_s{(total_bytes / (1024.0 * 1024.0 * 1024.0)) / seconds};

    if(!all_ok)
    {
        std::fprintf(stderr, "warning: validation disagreement detected for scenario=%s impl=%s; " "results may not be comparable\n", cfg.name, impl_name);
    }

    bench_result r{};
    r.scenario = cfg.name;
    r.impl = impl_name;
    r.bytes = buffer.size();
    r.total_ns = total_ns;
    r.ns_per_byte = ns_per_byte;
    r.gib_per_s = gib_per_s;
    return r;
}

static void print_bench_result(bench_result const& r)
{
    // Single machine-readable line; external scripts only need this format.
    std::printf(
        "uwvm2_utf scenario=%s impl=%s bytes=%zu total_ns=%lld " "ns_per_byte=%.6f gib_per_s=%.6f\n",
        r.scenario,
        r.impl,
        static_cast<std::size_t>(r.bytes),
        static_cast<long long>(r.total_ns),
        r.ns_per_byte,
        r.gib_per_s);
}

int main()
{
    // Scale knobs:
    //
    //   UTF_BENCH_BYTES : approximate bytes per scenario buffer (default: 16 MiB)
    //   ITERS           : number of outer iterations               (default: 20)
    //
    std::size_t const target_bytes = read_env_size("UTF_BENCH_BYTES", 16u * 1024u * 1024u);
    std::size_t const iterations = read_env_size("ITERS", 20u);
    std::size_t const warmup_iterations = read_env_size("WARMUP_ITERS", 1u);
    std::size_t const trials = read_env_size("TRIALS", 3u);

    scenario_config const scenarios[]{
        {"ascii_only",  scenario_kind::ascii_only },
        {"latin_mixed", scenario_kind::latin_mixed},
        {"emoji_mixed", scenario_kind::emoji_mixed},
    };

    std::puts("uwvm2 utf8 verification vs simdutf benchmark");
    std::printf("  target_bytes ≈ %zu per scenario\n", target_bytes);
    std::printf("  iterations   = %zu\n", iterations);

    for(auto const& cfg: scenarios)
    {
        auto const buffer{make_buffer(cfg, target_bytes)};

        bench_result agg_uwvm{};
        bench_result agg_simd{};
        bool have_uwvm{};
        bool have_simd{};
        double sum_ns_per_byte_uwvm{};
        double sum_ns_per_byte_simd{};
        double sum_gib_per_s_uwvm{};
        double sum_gib_per_s_simd{};

        for(std::size_t t{}; t < trials; ++t)
        {
            bool const uwvm_first = (t % 2u) == 0u;

            if(uwvm_first)
            {
                auto const r_uwvm{run_benchmark(cfg, "uwvm2", true, buffer, iterations, warmup_iterations)};
                auto const r_simd{run_benchmark(cfg, "simdutf", false, buffer, iterations, warmup_iterations)};

                if(!have_uwvm)
                {
                    agg_uwvm = r_uwvm;
                    have_uwvm = true;
                }
                if(!have_simd)
                {
                    agg_simd = r_simd;
                    have_simd = true;
                }

                sum_ns_per_byte_uwvm += r_uwvm.ns_per_byte;
                sum_ns_per_byte_simd += r_simd.ns_per_byte;
                sum_gib_per_s_uwvm += r_uwvm.gib_per_s;
                sum_gib_per_s_simd += r_simd.gib_per_s;
            }
            else
            {
                auto const r_simd{run_benchmark(cfg, "simdutf", false, buffer, iterations, warmup_iterations)};
                auto const r_uwvm{run_benchmark(cfg, "uwvm2", true, buffer, iterations, warmup_iterations)};

                if(!have_uwvm)
                {
                    agg_uwvm = r_uwvm;
                    have_uwvm = true;
                }
                if(!have_simd)
                {
                    agg_simd = r_simd;
                    have_simd = true;
                }

                sum_ns_per_byte_uwvm += r_uwvm.ns_per_byte;
                sum_ns_per_byte_simd += r_simd.ns_per_byte;
                sum_gib_per_s_uwvm += r_uwvm.gib_per_s;
                sum_gib_per_s_simd += r_simd.gib_per_s;
            }
        }

        if(trials > 0u)
        {
            double const inv_trials{1.0 / static_cast<double>(trials)};
            agg_uwvm.ns_per_byte = sum_ns_per_byte_uwvm * inv_trials;
            agg_simd.ns_per_byte = sum_ns_per_byte_simd * inv_trials;
            agg_uwvm.gib_per_s = sum_gib_per_s_uwvm * inv_trials;
            agg_simd.gib_per_s = sum_gib_per_s_simd * inv_trials;
        }

        print_bench_result(agg_uwvm);
        print_bench_result(agg_simd);
    }

    return 0;
}
