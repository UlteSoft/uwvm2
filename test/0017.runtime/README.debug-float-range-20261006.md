# Floating literal range consistency — 2026-10-06

uwvm2 and uwvm2-ros now reject the same out-of-range decimal floating literals
in the DAP syntax validator as their existing C++ source evaluator.
Previously Python's binary64 check accepted `3.5e38f`, `1e39F` and a
binary32 overflow midpoint, while the actual C++ parser rejected them.
Nonzero inputs that round to zero, such as `1e-999f` and `1e-999`, also
escaped the old DAP check.

This increment changes the shared DAP validator, its corpus and test recipes.
The production C++ scalar parser remains
`c512c7602fbc58275e837c83eb9c81cc2f335f9532c80aa7d2a5026207e46ecb`.
Its conversion and range policy still use `fast_io::parse_by_scan`.
No guest write, call, pointer, stop identity or ASM/VM debugging capability
is created.

The f/F suffix selects binary32. Nearest-even overflow occurs at
`2^128 - 2^103`; a nonzero magnitude at or below `2^-150` rounds to zero.
Binary64 conversion can round decimal strings on either side onto the same
binary32 boundary. The validator therefore compares the original normalized
decimal exactly when the binary64 image equals a boundary.
[Python Decimal documentation](https://docs.python.org/3/library/decimal.html)
describes exact string/integer construction, construction independent of
context precision, and explicit exact `Decimal.from_float` conversion.
The implementation performs no Decimal arithmetic or context mutation.
Other values use bounded binary64 comparisons. Apostrophes and underscores
are removed only for numeric validation; the original expression continues
to the authenticated controller.

For unsuffixed binary64, a nonzero significand that Python rounds to zero is
rejected. Actual zero significands remain valid even with a large exponent.
This preserves the existing fast_io/C++ literal range-error policy; it does
not claim complete language-specific Go, Rust or Zig constant semantics.
Floating arithmetic and copied NaN/infinity values are outside this literal
syntax check.

Examples:

| Expression | Result |
| --- | --- |
| `3.5e38f` | DAP rejects before broker I/O; C++ already rejected |
| `3.5e38` | Valid finite binary64 literal |
| `340282356779733661637539395458142568447.0f` | Valid; rounds to maximum finite binary32 |
| `340282356779733661637539395458142568448.0f` | Overflow midpoint; rejected |
| `3.402_823_5e3_8f` | Valid separated literal; rounds to maximum finite binary32 |
| `1.401298464324817e-45f` | Valid; minimum nonzero binary32 |
| `1e-999f` | Nonzero underflow; rejected |
| `0.0e999f` | Actual zero; accepted |
| `0 && 3.5e38f` | Invalid literal syntax still rejected in a dead branch |

At an actual authenticated existing source stop, commands can include:

```text
print-frame THREAD STOP FRAME 3.402_823_5e3_8f
print-frame THREAD STOP FRAME 3.5e38
print-frame THREAD STOP FRAME 1.401298464324817e-45f
```

Use the actual current THREAD, STOP and FRAME identities. These examples do
not establish a live source stop or synthesize guest read authority.

## Actual qualification

| Actual run | uwvm2 | uwvm2-ros |
| --- | --- | --- |
| Native guarded phases | 15 / 15 | 15 / 15 |
| Source-frame DAP protocol tests | 33 / 33 | 33 / 33 |
| ppc64 Linux ELF64 big-endian QEMU | 3 / 3 components | 3 / 3 components |
| x86_64 Linux ELF64 little-endian QEMU | 3 / 3 components | 3 / 3 components |

All six jobs passed. QEMU runs the real paired public component registry for
`debug_source_float_range`, `debug_source_float_separator` and
`debug_source_dap_expression`. Each actual output equals the same repository's
pinned native reference; x86_64 also directly executes its target ELF.
Native regression additionally covers scalar properties, Boolean casts,
ordinary scalar arithmetic and conditionals.

The new verifier generates 1,230 literals per job with an independent integer
numerator/denominator IEEE nearest-even encoder. No Python float conversion,
Decimal arithmetic, fast_io converter or production parser supplies its
expected bits. Across six jobs, 7,380 literals generate 14,760 actual C++
result rows, with Wasm32 and Wasm64 ABI settings checked independently.
Both overflow and underflow midpoints are sampled from each side with long
decimal tails that share a binary64 image. The set also includes f/F and
unsuffixed forms, fractional/exponent spellings, separators, subnormals,
zero and values near precision limits.

Each job preserves 70 inputs that legitimately round to maximum finite
binary32. The existing separator fixture also passes 3,947,904 checks across
six primary executions. Every literal probe verifies zero value/type resolver
callbacks, and the C++ fixture retains compiler-produced finite boundary bits
as a separate oracle.

The actual old DAP adapter incorrectly accepts 408 out-of-range binary32
cases per native repository, 816 in total. The new validator rejects them.
The corpus now has 230 positive and 182 negative expressions, adding nine
positive and 50 negative cases. Actual DAP frame protocol tests require every
negative expression to fail without sending a broker command, including
short-circuit and unselected conditional operands.

All 1,230 generated inputs are additionally validated under Decimal precision
1, ROUND_UP, Emin/Emax -1/1 and all traps enabled. The validator leaves sticky
flags unchanged. This tests independence from a caller's Decimal context.
25,486 input-hash checks completed; both production cuts and actual compiler
dependencies, link providers, QEMU binaries, ELFs and logs remain unchanged.

Frozen production source IDs:

- uwvm2: `sha256:e16d76f6ba97223d5dcfadf1b47923fa9cd51d84e26614e029364896edc27a6a`
- uwvm2-ros: `sha256:f502154ca8b3adcb0a8b2be6aea046b098edbcb77c83c8a3e3a3a5847e65d548`

Other agents' later changes to unrelated DAP reply observation are preserved.
The qualified source-evaluation function remains byte-identical in the current
paired files. This qualification does not cover those other changes.

The first fixture incorrectly expected `1e-999f` to be accepted. The failed
guard and a separately guarded diagnostic retain the actual C++ refusal.
The corrected fixture, underflow checks and corpus use a new immutable cut;
verified hardlinks reuse unchanged production inputs. The original cross
task labels contained x86_64's underscore and failed the harness label
preflight after four successful jobs. Only the two pending task labels were
corrected; their exact argv and source remain unchanged, and both jobs passed.
All raw receipts and diagnostics remain retained.

A transient local ENOSPC refused opening the adapter for a write; both paired
files retained their complete previous hashes. Only two task-owned, SHA-verified
redundant upload archives already retained on Linux were removed. Subsequent
edits used atomic replacement with a before-content comparison.

## Bounds and evidence

Every compiler and test executable ran in the existing shared 64 GiB cgroup
`uwvm-debug-tests64g-20261006-r2`, swap zero, worker CPUs 16–31.
Six successful guarded jobs took 343.765 seconds in total; this is a shared
environment observation, not a benchmark. Peak aggregate owned RSS was
782,168,064 bytes, below the 1 GiB DATA cap. Observed writer output was at most
19,249,137 bytes, below 64 MiB. No OOM events increased, and all owned process
trees were retired/reaped.

The ext4 arena retains a 16 GiB hard image limit. Its observed physical
allocation before archiving was 1,019,482,112 bytes. Existing admission and
filesystem guards remain: 6 GiB shared DATA headroom, 1 GiB real filesystem
reserve, 4096 free inodes, 25 GiB host disk reserve, 8 MiB individual files and
1 MiB logs. Full-product admission still requires 9 GiB headroom.
The already pinned SDK and emulators are reused read-only.

Main Linux archive:

`/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/float-range-a2/float-range-evidence-a1.tar.xz`

Local copy:

`/Users/liyinan/.codex/artifacts/uwvm2-float-range-evidence-20261006-a1/float-range-evidence-a1.tar.xz`

The archive is 34,405,604 bytes with 12,751 regular members and SHA-256
`18b35b34dc5ecfc38fd2368e66d0e2cabbb5f28780cabb1373cd7d66c3dbdc26`.
Every member hash is verified on both hosts. It contains both immutable cuts,
the original upload archives, actual recipes, target/native ELFs, generated
cases, logs, successful/failed receipts, old adapters, scoped source review
and copied QEMU binaries. The reused SDK has its regular file hashes rechecked
and refers to its prior independently verified full provider archive
`fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`.
Final sidecars retain paired documentation/current file review and copy proofs.

[Machine-readable qualification](debug_float_range_qualification_20261006.json)
records exact hashes, source cuts, limits and outcomes. This increment qualifies
literal range, DAP syntax/protocol DATA and the stated Linux components.
Live C/C++/Objective-C, Rust, Go/TinyGo, Zig or AssemblyScript producer sessions,
full CLI/module builds, other architectures and complete native debugger
parity remain outside this qualification.
