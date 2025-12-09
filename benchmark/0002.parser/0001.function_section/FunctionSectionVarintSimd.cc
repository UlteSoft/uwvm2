// FunctionSectionVarintSimd.cc
//
// Micro-benchmark for uwvm2's function_section LEB128<u32> decoder,
// following the general style of the Rust crate `varint-simd` decode benches:
//
// - generate one large contiguous buffer of LEB128<u32>-encoded type indices;
// - repeatedly decode the entire buffer;
// - report average time per value (ns) and throughput in GiB/s;
// - cover several typeidx ranges:
//     * u8_1b  : typeidx < 2^7, always 1-byte encodings,
//                mapped to scan_function_section_impl_u8_1b.
//                This is a specialized fast path (SIMD + zero-copy u8 view)
//                and is NOT a 1:1 fair comparison against a generic decoder.
//     * u8_2b  : 2^7 <= typeidx < 2^8, 1–2 byte encodings,
//                mapped to scan_function_section_impl_u8_2b.
//                This is the main fair-comparison scenario against
//                varint-simd's varint-u8/decode.
//     * u16_2b : 2^8 <= typeidx < 2^14, 1–2 byte encodings,
//                mapped to scan_function_section_impl_u16_2b.
//                This decodes into a u16 array and is relatively rare in
//                real-world wasm modules; it mostly serves as a stress /
//                completeness scenario. A fully general u16 decoder comparable
//                to varint-u16/decode would correspond to a (not yet
//                implemented) u16_3b path.
//
// Machine-readable output lines look like:
//
//   uwvm2_fs scenario=<...> impl=<scalar|simd> values=<...> total_ns=<...> \
//            ns_per_value=<...> avg_bytes_value=<...> gib_per_s=<...>
//
// They are consumed by external scripts (Lua, shell, etc.) to compare with
// the Rust varint-simd results.
//
// Typical standalone build command (from project root):
//
//   clang++ benchmark/0002.parser/0001.function_section/FunctionSectionVarintSimd.cc \
//       -o benchmark/0002.parser/0001.function_section/FunctionSectionVarintSimd      \
//       -std=c++2c -O3 -ffast-math -march=native                                     \
//       -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables                \
//       -I src -I third-parties/fast_io/include -I third-parties/bizwen/include     \
//       -I third-parties/boost_unordered/include
//

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <random>
#include <chrono>
#include <cstdlib>

#include <uwvm2/parser/wasm/standard/wasm1/features/function_section.h>
// Bring in the full wasm1 feature definition (feature name, binfmt version, etc.).
#include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>

namespace wasm1_ns = ::uwvm2::parser::wasm::standard::wasm1;
namespace feat = ::uwvm2::parser::wasm::standard::wasm1::features;
namespace binfmt1 = ::uwvm2::parser::wasm::binfmt::ver1;
namespace wasm_concepts = ::uwvm2::parser::wasm::concepts;

using wasm1_feature = feat::wasm1;
using module_storage_t = binfmt1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1_feature>;
using feature_param_t = wasm_concepts::feature_parameter_t<wasm1_feature>;
using wasm_u32 = wasm1_ns::type::wasm_u32;

enum class scenario_kind
{
    u8_1b,
    u8_2b,
    u16_2b
};

struct scenario_config
{
    char const* name{};
    scenario_kind kind{};
    wasm_u32 type_section_count{};
};

struct bench_result
{
    char const* scenario{};
    char const* impl{};
    std::size_t values{};
    long long total_ns{};
    double ns_per_value{};
    double avg_bytes_value{};
    double gib_per_s{};
};

// Helper: encode a wasm_u32 (typeidx) as unsigned LEB128 into an output buffer.
static void uleb128_encode_u32(std::vector<std::byte>& out, wasm_u32 v)
{
    std::uint32_t value{static_cast<std::uint32_t>(v)};

    do
    {
        std::uint8_t byte{static_cast<std::uint8_t>(value & 0x7Fu)};
        value >>= 7u;
        if(value != 0u) { byte = static_cast<std::uint8_t>(byte | 0x80u); }
        out.push_back(static_cast<std::byte>(byte));
    } while(value != 0u);
}

// Generate a LEB128 stream for the given scenario.
// Values are drawn uniformly from [0, type_section_count).
static std::vector<std::byte> generate_stream(scenario_config cfg,
                                              std::size_t func_count,
                                              std::uint64_t seed)
{
    std::vector<std::byte> buf;
    // Reserve assuming 1–2 bytes per value as the common case.
    buf.reserve(func_count * 2u);

    std::mt19937_64 rng{seed};
    std::uniform_int_distribution<wasm_u32> dist{
        0u, static_cast<wasm_u32>(cfg.type_section_count - 1u)};

    for(std::size_t i{}; i < func_count; ++i)
    {
        wasm_u32 v{dist(rng)};
        uleb128_encode_u32(buf, v);
    }

    return buf;
}

// Scalar baseline: linear decode using fast_io's leb128_get with basic validation.
static void scalar_decode_uleb128_u32(std::byte const* begin,
                                      std::byte const* end,
                                      wasm_u32 type_section_count,
                                      wasm_u32 func_count)
{
    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 counter{};

    auto section_curr{begin};

    while(section_curr != end)
    {
        if(counter == func_count) [[unlikely]]
        {
            std::fprintf(stderr,
                         "scalar_decode: func_counter exceeded expected func_count\n");
            std::abort();
        }

        wasm_u32 value;  // no init

        auto const [next_ptr, err]{::fast_io::parse_by_scan(
            reinterpret_cast<char8_t const*>(section_curr),
            reinterpret_cast<char8_t const*>(end),
            ::fast_io::mnp::leb128_get(value))};

        if(err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            std::fprintf(stderr, "scalar_decode: parse_by_scan error\n");
            std::abort();
        }

        if(value >= type_section_count) [[unlikely]]
        {
            std::fprintf(stderr,
                         "scalar_decode: decoded value >= type_section_count\n");
            std::abort();
        }

        ++counter;
        section_curr = reinterpret_cast<std::byte const*>(next_ptr);
    }

    if(counter != func_count)
    {
        std::fprintf(stderr,
                     "scalar_decode: func_counter mismatch (got %u, expect %u)\n",
                     static_cast<unsigned>(counter),
                     static_cast<unsigned>(func_count));
        std::abort();
    }
}

// One SIMD decode pass for a given scenario, dispatching to the corresponding
// scan_function_section_impl_* implementation in function_section.
static void simd_decode_once(scenario_config cfg,
                             module_storage_t& module,
                             feature_param_t const& fs_para,
                             std::byte const* begin,
                             std::byte const* end,
                             wasm_u32 func_count)
{
    using function_section_storage_t = feat::function_section_storage_t;

    auto& functionsec{
        wasm_concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module.sections)};

    // Clear results from previous iterations so that the underlying vectors
    // do not grow without bound across iterations.
    functionsec.funcs.clear();

    wasm_u32 func_counter{};
    ::uwvm2::parser::wasm::base::error_impl err{};

    auto const sec_adl =
        wasm_concepts::feature_reserve_type_t<function_section_storage_t>{};

    ::std::byte const* result_ptr{};

    switch(cfg.kind)
    {
        case scenario_kind::u8_1b:
        {
            result_ptr = feat::scan_function_section_impl_u8_1b(
                sec_adl, module, begin, end, err, fs_para, func_counter, func_count);
            break;
        }
        case scenario_kind::u8_2b:
        {
            result_ptr = feat::scan_function_section_impl_u8_2b(
                sec_adl, module, begin, end, err, fs_para, func_counter, func_count);
            break;
        }
        case scenario_kind::u16_2b:
        {
            result_ptr = feat::scan_function_section_impl_u16_2b(
                sec_adl, module, begin, end, err, fs_para, func_counter, func_count);
            break;
        }
        default:
        {
            std::fprintf(stderr, "simd_decode_once: unknown scenario_kind\n");
            std::abort();
        }
    }

    if(result_ptr != end)
    {
        std::fprintf(stderr,
                     "simd_decode_once: scan_* did not consume all bytes "
                     "(%td bytes left)\n",
                     end - result_ptr);
        std::abort();
    }

    if(func_counter != func_count)
    {
        std::fprintf(stderr,
                     "simd_decode_once: func_counter mismatch (got %u, expected %u)\n",
                     static_cast<unsigned>(func_counter),
                     static_cast<unsigned>(func_count));
        std::abort();
    }
}

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

static bench_result run_benchmark(scenario_config cfg,
                                  char const* impl_name,
                                  bool use_simd,
                                  std::size_t func_count,
                                  std::size_t iterations)
{
    // 1. Generate input buffer
    auto const buffer{
        generate_stream(cfg, func_count, 0x1234'5678'9ABC'DEF0ull)};

    auto const* const begin{buffer.data()};
    auto const* const end{buffer.data() + buffer.size()};

    // 2. Module storage and feature parameters (used only by the SIMD path)
    module_storage_t module{};
    feature_param_t fs_para{};

    using type_section_storage_t = feat::type_section_storage_t<wasm1_feature>;
    auto& typesec{
        wasm_concepts::operation::get_first_type_in_tuple<type_section_storage_t>(module.sections)};
    typesec.types.resize(static_cast<std::size_t>(cfg.type_section_count));

    // 3. Measure total decode time over all iterations
    auto const t0{std::chrono::steady_clock::now()};

    for(std::size_t i{}; i < iterations; ++i)
    {
        if(use_simd)
        {
            simd_decode_once(cfg,
                             module,
                             fs_para,
                             begin,
                             end,
                             static_cast<wasm_u32>(func_count));
        }
        else
        {
            scalar_decode_uleb128_u32(
                begin, end, cfg.type_section_count, static_cast<wasm_u32>(func_count));
        }
    }

    auto const t1{std::chrono::steady_clock::now()};

    long long const total_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()};

    std::size_t const total_values{func_count * iterations};
    double const ns_per_value{
        static_cast<double>(total_ns) / static_cast<double>(total_values)};

    double const avg_bytes_per_value{
        static_cast<double>(buffer.size()) / static_cast<double>(func_count)};

    double const total_bytes{
        static_cast<double>(buffer.size()) * static_cast<double>(iterations)};

    double const seconds{static_cast<double>(total_ns) * 1e-9};
    double const gib_per_s{(total_bytes / (1024.0 * 1024.0 * 1024.0)) / seconds};

    bench_result r{};
    r.scenario = cfg.name;
    r.impl = impl_name;
    r.values = total_values;
    r.total_ns = total_ns;
    r.ns_per_value = ns_per_value;
    r.avg_bytes_value = avg_bytes_per_value;
    r.gib_per_s = gib_per_s;
    return r;
}

static void print_bench_result(bench_result const& r)
{
    // Single machine-readable line; external scripts only need this format.
    std::printf(
        "uwvm2_fs scenario=%s impl=%s values=%zu total_ns=%lld "
        "ns_per_value=%.6f avg_bytes_value=%.6f gib_per_s=%.6f\n",
        r.scenario,
        r.impl,
        r.values,
        static_cast<long long>(r.total_ns),
        r.ns_per_value,
        r.avg_bytes_value,
        r.gib_per_s);
}

int main()
{
    // Adjustable scale, roughly aligned with varint-simd:
    //
    //   FUNC_COUNT : number of values decoded per run (default: 1_000_000)
    //   ITERS      : number of outer iterations        (default: 20)
    //
    std::size_t const func_count = read_env_size("FUNC_COUNT", 1'000'000);
    std::size_t const iterations = read_env_size("ITERS", 20);

    scenario_config const scenarios[]{
        // [0, 2^7)
        {"u8_1b", scenario_kind::u8_1b, static_cast<wasm_u32>(100u)},
        // [2^7, 2^8)
        {"u8_2b", scenario_kind::u8_2b, static_cast<wasm_u32>(200u)},
        // [2^8, 2^14)
        {"u16_2b", scenario_kind::u16_2b, static_cast<wasm_u32>(2048u)},
    };

    std::puts("uwvm2 function_section uleb128<u32> SIMD benchmark (varint-simd style)");
    std::printf("  func_count  = %zu\n", func_count);
    std::printf("  iterations  = %zu\n", iterations);

    for(auto const& cfg: scenarios)
    {
        // baseline scalar
        auto const r_scalar{
            run_benchmark(cfg, "scalar", false, func_count, iterations)};
        print_bench_result(r_scalar);

        // function_section SIMD
        auto const r_simd{
            run_benchmark(cfg, "simd", true, func_count, iterations)};
        print_bench_result(r_simd);
    }

    return 0;
}


