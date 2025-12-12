# xxh3 UTF-8 Benchmark (uwvm2 vs upstream xxHash)

This directory contains a small micro-benchmark that compares the project’s
`uwvm2::utils::hash::xxh3_64bits` implementation against the upstream
xxHash `XXH3_64bits` on the same UTF-8 style text buffer.

- C++ benchmark: `Xxh3UtfBenchmark.cc`
- Lua driver   : `compare_xxh3_utf.lua`

The Lua script:

- clones the official [xxHash](https://github.com/Cyan4973/xxHash) repository
  into `benchmark/0001.utils/0003.xxh3/outputs/xxhash` (unless `XXHASH_DIR`
  is set to an existing checkout),
- builds a single C++ benchmark binary that links both uwvm2’s xxh3 and
  xxHash’s `XXH3_64bits` (header-only mode),
- runs the benchmark and prints a simple throughput comparison (GiB/s).

## Running the benchmark

From the project root:

```sh
lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
# or
xmake lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
```

Environment variables:

- `CXX` / `CXXFLAGS_EXTRA` – C++ compiler and extra flags (optional)
- `XXHASH_DIR` – custom path to an existing xxHash checkout
- `BYTES` – total size of the UTF-8 buffer in bytes (default: 16 MiB)
- `ITERS` – number of outer iterations (default: 50)
- `UWVM2_SIMD_LEVEL` – optional fixed SIMD level (same values as in other
  benchmarks, e.g. `native`, `avx2`, `avx512vbmi`, …).

The C++ binary prints lines of the form:

```text
xxh3_utf impl=uwvm2_xxh3 bytes=... total_ns=... gib_per_s=... checksum=...
xxh3_utf impl=xxhash_xxh3 bytes=... total_ns=... gib_per_s=... checksum=...
```

These are parsed by the Lua script to compute and display the relative
throughput of both implementations.

