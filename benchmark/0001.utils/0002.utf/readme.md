# UTF-8 Verification Benchmark (uwvm2::utils::utf vs simdutf)

This benchmark compares the performance of:

- `uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>`
- `simdutf::validate_utf8` from the upstream [simdutf](https://github.com/simdutf/simdutf) project

on several representative, fully valid UTF-8 workloads. The primary metric is
nanoseconds per byte (`ns_per_byte`), with derived throughput in GiB/s.

The benchmark is intentionally self-contained and driven from Lua so that it
can be reproduced outside of the main build system.

---

## Files in this directory

- `UtfSimdUtf.cc`  
  C++ benchmark binary. Generates test data, runs both implementations, and
  prints machine-readable timing lines.

- `compare_utf_simdutf.lua`  
  Lua driver that:
  - locates or clones a `simdutf` checkout;
  - compiles `UtfSimdUtf.cc` together with `simdutf/src/simdutf.cpp`;
  - runs the benchmark; and
  - writes summaries under `outputs/`.

- `outputs/` (ignored via `.gitignore`)  
  - `UtfSimdUtf` – compiled benchmark binary  
  - `utf_bench.log` – full stdout/stderr from the benchmark run  
  - `utf_bench_summary.txt` – per-scenario `ns_per_byte` summary  
  - `simdutf/` – auto-cloned simdutf repository (if `SIMDUTF_DIR` is not set)

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

The Lua driver will:

1. Discover `simdutf`:
   - If `SIMDUTF_DIR` is set, use that directory (it must contain a simdutf
     checkout).
   - Otherwise, clone simdutf into
     `benchmark/0001.utils/0002.utf/outputs/simdutf`.
2. Determine the simdutf public headers:
   - If `<SIMDUTF_DIR>/singleheader/simdutf.h` exists, use `singleheader/`.
   - Otherwise, use `<SIMDUTF_DIR>/include`.
3. Compile the C++ benchmark:
   - Sources:
     - `UtfSimdUtf.cc`
     - `<SIMDUTF_DIR>/src/simdutf.cpp`
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

The compiled binary is placed at:

- `benchmark/0001.utils/0002.utf/outputs/UtfSimdUtf`

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

For each scenario, the benchmark:

1. Runs `uwvm2::utils::utf::check_legal_utf8_rfc3629_unchecked<false>` over
   the entire buffer `ITERS` times.
2. Runs `simdutf::validate_utf8` over the same buffer `ITERS` times.
3. Verifies that both implementations agree on validity.

Metrics:

- `ns_per_byte` – average time per input byte across all iterations.
- `gib_per_s` – effective throughput derived from the total bytes processed
  and total time.

---

## Output format

`UtfSimdUtf` prints one machine-readable line per (scenario, implementation)
pair:

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

