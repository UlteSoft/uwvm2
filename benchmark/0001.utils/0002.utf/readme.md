# UTF-8 Verification Benchmark (uwvm2::utils::utf vs simdutf)

This benchmark compares the performance of:

- `uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>`
- `simdutf::validate_utf8` from the upstream [simdutf](https://github.com/simdutf/simdutf) project

on several representative, fully valid UTF-8 workloads. The primary metric is
nanoseconds per byte (`ns_per_byte`), with derived throughput in GiB/s.

For fairness and reproducibility, the results in this directory correspond to:

- uwvm2 (this repo): `0c6692e2d48a0f065d51f52a3ede07b5b26e4edf`  
  https://github.com/UlteSoft/uwvm2/commit/0c6692e2d48a0f065d51f52a3ede07b5b26e4edf
- simdutf (upstream): `b34887b88794390adfa460f5d502a74d8b1f9cd1`  
  https://github.com/simdutf/simdutf/commit/b34887b88794390adfa460f5d502a74d8b1f9cd1

The benchmark is intentionally self-contained and driven from Lua so that it
can be reproduced outside of the main build system.

---

## Files in this directory

- `UtfGen.cc`  
  C++ data generator. Produces per-scenario UTF-8 input files for the benches.

- `UtfUwvm2Bench.cc`  
  C++ benchmark binary for `uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>`.

- `UtfSimdutfBench.cc`  
  C++ benchmark binary for `simdutf::validate_utf8`.

- `UtfSimdUtf.cc`  
  Legacy combined benchmark (both implementations in one process). Kept for
  reference; the Lua driver uses the per-implementation binaries above.

- `compare_utf_simdutf.lua`  
  Lua driver that:
  - locates or clones a `simdutf` checkout;
  - builds one generator + two per-implementation benchmark binaries;
  - generates per-scenario input files;
  - runs each benchmark in a separate process; and
  - writes summaries under `outputs/`.

- `outputs/` (ignored via `.gitignore`)  
  - `UtfGen` – compiled data generator  
  - `UtfUwvm2Bench` – compiled uwvm2 benchmark binary  
  - `UtfSimdutfBench` – compiled simdutf benchmark binary  
  - `utf_bench.log` – full stdout/stderr from the benchmark run  
  - `utf_bench_summary.txt` – per-scenario `ns_per_byte` summary  
  - `simdutf/` – auto-cloned simdutf repository (if `SIMDUTF_DIR` is not set)

- `compare_utf_simdutf.py`  
  Python driver equivalent to `compare_utf_simdutf.lua` (same behavior and
  environment variables).

---

## Building and running

From the project root:

```bash
cd benchmark/0001.utils/0002.utf

# Typical invocation (with cross toolchain sysroot + lld):
CXXFLAGS_EXTRA="--sysroot=$SYSROOT -fuse-ld=lld" \
lua compare_utf_simdutf.lua

# or, via xmake's Lua runner
CXXFLAGS_EXTRA="--sysroot=$SYSROOT -fuse-ld=lld" \
xmake lua benchmark/0001.utils/0002.utf/compare_utf_simdutf.lua
```

Or using Python:

```bash
cd benchmark/0001.utils/0002.utf
CXXFLAGS_EXTRA="--sysroot=$SYSROOT -fuse-ld=lld" \
python3 compare_utf_simdutf.py
```

The driver will:

1. Discover `simdutf`:
   - If `SIMDUTF_DIR` is set, use that directory (it must contain a simdutf
     checkout).
   - Otherwise, clone simdutf into
     `benchmark/0001.utils/0002.utf/outputs/simdutf`.
2. Determine the simdutf public headers:
   - If `<SIMDUTF_DIR>/singleheader/simdutf.h` exists, use `singleheader/`.
   - Otherwise, use `<SIMDUTF_DIR>/include`.
3. Compile the C++ programs:
   - Sources:
     - `UtfGen.cc`
     - `UtfUwvm2Bench.cc`
     - `UtfSimdutfBench.cc`
     - `<SIMDUTF_DIR>/src/simdutf.cpp` (only for the simdutf bench)
   - Includes:
     - `-I <project-root>/src`
     - `-I <project-root>/third-parties/fast_io/include`
     - `-I <project-root>/third-parties/bizwen/include`
     - `-I <project-root>/third-parties/boost_unordered/include`
     - `-I <SIMDUTF_INCLUDE>` (either `singleheader` or `include`)
     - `-I <SIMDUTF_DIR>/src` (for simdutf’s internal headers)
4. Run the benchmark and capture output:
   - raw log: `outputs/utf_bench.log`
   - summary: `outputs/utf_bench_summary.txt`

The compiled binaries are placed under:

- `benchmark/0001.utils/0002.utf/outputs/UtfGen`
- `benchmark/0001.utils/0002.utf/outputs/UtfUwvm2Bench`
- `benchmark/0001.utils/0002.utf/outputs/UtfSimdutfBench`

---

## Environment variables

### Compiler and toolchain

- `CXX`  
  C++ compiler to use (default: `clang++`).

- `CXXFLAGS_EXTRA`  
  Extra C++ flags appended after the defaults. This is the preferred place to
  pass sysroot and linker settings for cross/SDK-based toolchains, e.g.:

  ```bash
  CXXFLAGS_EXTRA="--sysroot=$SYSROOT -fuse-ld=lld"
  ```

### simdutf location

- `SIMDUTF_DIR`  
  Path to an existing simdutf checkout. If unset, the Lua driver will clone
  simdutf into `outputs/simdutf` on first use.

- `SIMDUTF_INCLUDE`  
  Optional override for the directory that contains `simdutf.h`. If unset, the
  driver auto-detects one of:
  - `<SIMDUTF_DIR>/singleheader`
  - `<SIMDUTF_DIR>/include`

### Workload size and iterations

- `UTF_BENCH_BYTES`  
  Approximate number of bytes per scenario buffer. The benchmark repeats a
  scenario-specific UTF-8 snippet until this size is reached or exceeded.  
  Default: `16 * 1024 * 1024` (16 MiB).

- `ITERS`  
  Number of iterations per scenario (each iteration validates the entire
  buffer).  
  Default: `20`.

- `WARMUP_ITERS`  
  Warm-up iterations per implementation (not timed) before each measured run.  
  Default: `1`.

- `TRIALS`  
  Number of measurement trials per scenario. The benchmark alternates which
  implementation runs first each trial and averages results.  
  Default: `3`.

### SIMD configuration (uwvm2 side)

- `UWVM2_SIMD_LEVEL`  
  Optional fixed SIMD level for the C++ build, shared with other benchmarks in
  this repository. Recognized values:

  - `native` (default)
  - `sse2`, `sse3`, `ssse3`
  - `sse4`
  - `avx`, `avx2`
  - `avx512bw`
  - `avx512vbmi`

The Lua driver maps these values to appropriate `-march` / `-m*` flags for
`UtfSimdUtf.cc` and simdutf.

---

## Workloads and metrics

The C++ benchmark currently defines three fully valid UTF-8 workloads:

- `ascii_only`  
  ASCII-only text (e.g., WASI paths, HTTP-like headers, simple English text).

- `latin_mixed`  
  Primarily Latin-1-style accented characters (e.g., café, Straße, Español),
  simulating Western European text.

- `emoji_mixed`  
  Includes 4-byte code points (Emoji) and CJK snippets, approximating chat /
  social content.

Each scenario constructs a small UTF-8 “chunk” and repeats it until the buffer
is larger than `UTF_BENCH_BYTES`. Chunks are never cut in the middle, so the
resulting buffer stays valid UTF-8 by construction.

For each scenario, the benchmark runs `TRIALS` trials. In each trial it:

1. Warms up each implementation for `WARMUP_ITERS` iterations (not timed).
2. Measures each implementation over the entire buffer `ITERS` times, with
   the run order alternating per trial.
3. Averages timing metrics across trials and verifies both implementations agree.

Metrics:

- `ns_per_byte` – average time per input byte across all iterations.
- `gib_per_s` – effective throughput derived from the total bytes processed
  and total time.

---

## Output format

The per-implementation benchmarks print one machine-readable line per scenario:

```text
uwvm2_utf scenario=<name> impl=<uwvm2|simdutf> bytes=<N> total_ns=<T> \
          ns_per_byte=<ns> gib_per_s=<GiBps>
```

Example:

```text
uwvm2_utf scenario=ascii_only impl=uwvm2   bytes=16777216 total_ns=123456789 ns_per_byte=7.360000 gib_per_s=2.130000
uwvm2_utf scenario=ascii_only impl=simdutf bytes=16777216 total_ns= 98765432 ns_per_byte=5.890000 gib_per_s=2.660000
```

The Lua driver parses these lines from `utf_bench.log` and writes a compact
summary to `utf_bench_summary.txt`:

```text
scenario=ascii_only impl=uwvm2   ns_per_byte=7.360000
scenario=ascii_only impl=simdutf ns_per_byte=5.890000
scenario=emoji_mixed impl=uwvm2   ns_per_byte=...
scenario=emoji_mixed impl=simdutf ns_per_byte=...
```

It then prints a human-readable comparison per scenario:

```text
Scenario: ascii_only
  uwvm2  : ns_per_byte = 7.360000
  simdutf: ns_per_byte = 5.890000
  ratio  : uwvm2 / simdutf ≈ 1.250  (<1 means uwvm2 is faster)
```

---

## Sample results (aarch64-apple-darwin, Apple M4)

Single run results (unit: `ns_per_byte`):

| scenario | uwvm2 ns/byte | simdutf ns/byte | ratio (uwvm2/simdutf) |
|----------|--------------:|----------------:|----------------------:|
| ascii_only  | 0.018696 | 0.021644 | 0.864 |
| emoji_mixed | 0.074537 | 0.065481 | 1.138 |
| latin_mixed | 0.074210 | 0.070943 | 1.046 |

---

## Sample results (x86_64-linux, Intel Core i9-14900HK)

Single run results across `UWVM2_SIMD_LEVEL` (unit: `ns_per_byte`):

| UWVM2_SIMD_LEVEL | scenario | uwvm2 ns/byte | simdutf ns/byte | ratio (uwvm2/simdutf) |
|------------------|----------|--------------:|----------------:|----------------------:|
| sse2  | ascii_only  | 0.028489 | 0.022947 | 1.242 |
| sse2  | emoji_mixed | 0.300820 | 0.316528 | 0.950 |
| sse2  | latin_mixed | 0.188053 | 0.237135 | 0.793 |
| sse3  | ascii_only  | 0.023575 | 0.022317 | 1.056 |
| sse3  | emoji_mixed | 0.281868 | 0.304088 | 0.927 |
| sse3  | latin_mixed | 0.184568 | 0.227135 | 0.813 |
| ssse3 | ascii_only  | 0.022218 | 0.024373 | 0.912 |
| ssse3 | emoji_mixed | 0.089772 | 0.303649 | 0.296 |
| ssse3 | latin_mixed | 0.089216 | 0.227636 | 0.392 |
| sse4  | ascii_only  | 0.024462 | 0.018325 | 1.335 |
| sse4  | emoji_mixed | 0.088551 | 0.085584 | 1.035 |
| sse4  | latin_mixed | 0.089386 | 0.088902 | 1.005 |
| avx   | ascii_only  | 0.020854 | 0.017991 | 1.159 |
| avx   | emoji_mixed | 0.087235 | 0.086666 | 1.007 |
| avx   | latin_mixed | 0.088487 | 0.091145 | 0.971 |
| avx2  | ascii_only  | 0.022570 | 0.018672 | 1.209 |
| avx2  | emoji_mixed | 0.051944 | 0.046067 | 1.128 |
| avx2  | latin_mixed | 0.049816 | 0.051097 | 0.975 |

---

## Implementation notes and current workaround

### simdutf ARM64 implementation

On some `aarch64-apple-darwin` toolchains (e.g., recent Apple-clang with
`-std=c++2b` / `-std=c++2c`), compiling simdutf’s ARM64 implementation may
trigger errors of the form:

```text
simdutf/arm64/simd.h:313:35: error: constexpr variable 'pair' must be initialized by a constant expression
  simdutf_constexpr int8x16x2_t pair = ...
```

This appears to be a limitation or bug at the intersection of:

- simdutf’s use of `constexpr` with NEON intrinsics; and
- the specific compiler / standard library combination on this platform.

To keep this benchmark self-contained and reproducible across environments, the
Lua driver currently passes:

```text
-DSIMDUTF_IMPLEMENTATION_ARM64=0
```

when compiling both `UtfSimdUtf.cc` and `simdutf/src/simdutf.cpp`. This forces
simdutf to use its generic fallback implementation instead of the ARM64-specific
SIMD kernels. Functionality is unchanged; only the simdutf side of the benchmark
runs without ARM64 SIMD acceleration.

Once the underlying issue is resolved upstream, this macro can be removed to
restore a fully SIMD-accelerated comparison on ARM64.
