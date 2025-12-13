# xxh3 UTF-8 Benchmark (uwvm2 vs upstream xxHash)

This directory contains a small micro-benchmark that compares the project’s
`uwvm2::utils::hash::xxh3_64bits` implementation against the upstream
xxHash `XXH3_64bits` on the same UTF-8 style text buffer.

- C++ benchmarks:
  - `Xxh3UtfGen.cc` (generate data into a file)
  - `Xxh3UtfXxhashBench.cc` (benchmark upstream xxHash)
  - `Xxh3UtfUwvm2Bench.cc` (benchmark uwvm2)
- Drivers:
  - Lua   : `compare_xxh3_utf.lua`
  - Python: `compare_xxh3_utf.py`

The Lua script:

- clones the official [xxHash](https://github.com/Cyan4973/xxHash) repository
  into `benchmark/0001.utils/0003.xxh3/outputs/xxhash` (unless `XXHASH_DIR`
  is set to an existing checkout),
- builds three C++ binaries: one data generator and two per-implementation
  benchmark binaries (xxHash and uwvm2), all sharing the same input file,
- runs the generator once, then runs each benchmark in a separate process
  and prints a simple throughput comparison (GiB/s).

## Running the benchmark

From the project root:

```sh
lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
# or
xmake lua benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.lua
# or (Python)
python3 benchmark/0001.utils/0003.xxh3/compare_xxh3_utf.py
```

Environment variables:

- `CXX` / `CXXFLAGS_EXTRA` – C++ compiler and extra flags (optional)
- `XXHASH_DIR` – custom path to an existing xxHash checkout
- `BYTES` – total size of the UTF-8 buffer in bytes (default: 16 MiB)
- `ITERS` – number of outer iterations (default: 50)
- `DATA_FILE` – optional path for the generated input file
  (default: `outputs/xxh3_utf_data.bin` when using the Lua driver)
- `UWVM2_SIMD_LEVEL` – optional fixed SIMD level (same values as in other
  benchmarks, e.g. `native`, `avx2`, `avx512vbmi`, …).

The benchmark binaries print lines of the form:

```text
xxh3_utf impl=uwvm2_xxh3 bytes=... total_ns=... gib_per_s=... checksum=...
xxh3_utf impl=xxhash_xxh3 bytes=... total_ns=... gib_per_s=... checksum=...
```

These are parsed by the Lua script to compute and display the relative
throughput of both implementations.
