# fast_io sto vs fast_float integer parsing

This directory contains the full integer string-to-`uint64_t` benchmark requested for `fast_io` sto and `fast_float::from_chars`.

Checked-in result files:

- Full TSV: `results/sto_fast_float_m4_full.tsv`
- Full Markdown report: `results/sto_fast_float_m4_full.md`

Machine used for the checked-in result:

- Target/host: aarch64-apple-darwin, Apple M4
- Compiler: clang++ 23.0.0git

Coverage:

- Bases: 2 through 36
- Lengths: 1 through the maximum non-overflowing `uint64_t` digit length for each base
- Cases: bases 2-10 use `normal`; bases 11-36 use `lower`, `upper`, and `mixed`
- Extra trailing buffer: `0` and `64` bytes after each parsed string; `last` is always `first + length`
- Algorithms: `fast_float`, `fast_io_public`, `fast_io_mask`, `fast_io_scalar`

Build command:

```sh
clang++ -o /tmp/sto_fast_float_bench_full \
  benchmark/0001.utils/0004.sto_fast_float/sto_fast_float_bench.cc \
  -Ofast -fno-exceptions -flto -s -march=native -std=c++26 -fno-rtti \
  -I "$MACROMODEL/src/uwvm2/third-parties/fast_io/include" \
  -I "$MACROMODEL/src/uwvm2/third-parties/fast_float/include" \
  --sysroot="$SYSROOT" -fuse-ld=lld
```

Run command:

```sh
/tmp/sto_fast_float_bench_full --samples=16384 --iters=16 --repeats=5 \
  > benchmark/0001.utils/0004.sto_fast_float/results/sto_fast_float_m4_full.tsv
```

The full checked-in run produced 11,544 data rows for 2,886 input cases. Checksum validation found 0 mismatches across algorithms and 0 mismatches between the 0-byte and 64-byte buffer layouts.

Overall geomean ratios relative to `fast_float`:

| algorithm | geomean vs fast_float | slower than fast_float by >3% | faster than fast_float by >3% |
|---|---:|---:|---:|
| fast_io_public | 0.861 | 487/2886 | 1221/2886 |
| fast_io_mask | 0.730 | 50/2886 | 2760/2886 |
| fast_io_scalar | 0.860 | 193/2886 | 2539/2886 |

See `results/sto_fast_float_m4_full.md` for the per-buffer, per-base, worst-case, selected-row, and complete row-by-row Markdown tables.
