# fast_io scalar and binary codec audit

This suite compares a frozen pre-edit source snapshot with the current production
headers. `prepare.py` copies both versions and records SHA-256 identities. The
LLVM immediate helper is extracted verbatim from `single_func_emit.h` to exercise
its real constexpr and runtime body without linking the complete LLVM JIT.
This is a codec test, not a full-VM performance or conformance result.

The audit replaces fixed cache-header encoding/decoding, LZSS/native-LZ 16-bit
fields, cache byte appends, x87/68881 integer field decoding, LLVM fixed-width
immediates, and XXH3 scalar byte transport with fast_io. Integer transport retains
NaN payloads without materializing floating-point values. The hash and compression
algorithms and their wire formats are unchanged.

Global `src` searches also reviewed numeric text conversion, scalar LEB128,
byte-shift assembly, byte swapping and endian helpers. Existing fast_io-backed
WASI memory access stays in place. Specialized UTF-8 validation and SIMD LEB128
retain their existing contracts. In particular, `CHAR_BIT > 8` octet-by-octet
fallbacks cannot be replaced with the current fast_io fixed-width serializer:
its reserve length is expressed in native storage units, while those Wasm paths
interpret only the low eight bits of each unit. These fallback targets and mixed
endian machines were not executed; they are not covered by the measured results.

## Correctness

- A literal 64-byte expected header image, all 16 input alignments and all header
  truncation lengths; failed scalar reads retain destination and cursor.
- All 65,536 LZSS token images; round trips over empty, short, repeated and random
  data; a deterministic malformed corpus also compares partial decoder output.
- Signaling/quiet NaN, infinity, signed-zero and high-bit integer payloads.
- Constant-evaluated LLVM immediate and XXH3 byte access; seeded hashes compared
  across both implementations and all tested ABIs.
- O3, ASan/UBSan with leak checks, no exceptions; actual QEMU execution on s390x
  (big endian, 64-bit), i686 (little endian, 32-bit) and AArch64 (little endian,
  64-bit). Existing `test/0017.runtime/strict_float.cc` runs on all four targets.
  The modified 68881 branch is not an executed m68k target in this matrix.
- `modules.sh` compiles the actual fast_io, intrinsics, hash, container, cache
  format and cache compression module graph and a constexpr module consumer.

## Reproduction

Before applying the patch, save the source files listed in `prepare.py`, retaining
their repository-relative paths. Then run locally:

```sh
python3 test/0014.llvm_jit/fast_io_audit/prepare.py \
  --baseline /path/to/pre-edit-snapshot --out /path/to/comparison
```

Use the SSH Linux container with a hard 8 GiB memory limit, no swap and verified
E-cores 16–19. Transfer the source tree and prepared `comparison/` into `/scratch`.
The runners expect the project's existing `/toolchain` and `/work/deps` mounts,
full-target `llc` at `/work/artifacts/uwvm2-ros-jit/llvm/bin/llc`, and the C development
headers/startup objects under `/scratch/system`. With the minimal Debian image,
copy those from the existing development container with symlinks dereferenced;
the copied libc linker script must point at its copied `libc_nonshared.a`.
Required host tool runtime libraries live under `/scratch/host-libs`.

```sh
bash /scratch/test/0014.llvm_jit/fast_io_audit/run.sh /scratch /scratch/results
bash /scratch/test/0014.llvm_jit/fast_io_audit/modules.sh /scratch /scratch/modules
```

The run saves commands, logs and cgroup limits/peak/events. Benchmarks use one
E-core (CPU 16), three interleaved old/new batches with alternating order and nine
samples per batch. Compiler barriers expose produced bytes and decoded objects;
checksums must agree. Reported ratios use medians of all 27 thread CPU time samples
(`CLOCK_THREAD_CPUTIME_ID`); wall times are also retained. `benchmark.sh` can repeat
timing independently without rerunning the compiler/correctness matrix. This is a
shared host: small differences should be treated as noise, not promised speedups.
The identical baseline binary exhibited bimodal native-LZ decode times (roughly
11–30 microseconds). CPU time did not eliminate that variation. No stable
native-LZ encode/decode acceleration or regression is established by this run;
the raw samples must not be reduced to a whole-VM performance claim.

After copying the output directory back:

```sh
python3 test/0014.llvm_jit/fast_io_audit/summarize.py /path/to/results \
  --out /path/to/summary.json
```

Pass `--timing /path/to/benchmark-output` when timing was rerun separately.
See `results-2026-09-23.json` for the measured samples and source fingerprints.

## 2026-09-23 results

All six old/new correctness configurations passed 76,722 checks with identical
fingerprint `6274f7a811c12814`. The actual cache/hash module graph passed too.
The container used 8 GiB, no swap, E-cores 16–19; peak memory was about 825 MiB,
with no OOM or cgroup limit events. Timing was pinned to E-core 16.

| Operation | Before CPU ns | After CPU ns | Ratio before/after |
|---|---:|---:|---:|
| 64-byte header encode | 42.774 | 7.935 | 5.39x |
| 64-byte header decode | 5.828 | 3.965 | 1.47x |
| 16 KiB byte append | 11011.274 | 289.553 | 38.03x |
| 16 KiB native-LZ encode | 35747.028 | 35740.224 | 1.00x |
| 16 KiB native-LZ decode | 29207.048 | 29520.613 | 0.99x; unstable samples |
