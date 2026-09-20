# Linux/QEMU correctness matrix

`run_linux_qemu_matrix.sh` runs existing binaries and fixtures; it does not build
the runtime or toolchain. Use `--help` for platform wrappers and resource caps.
The default is one target process and zero extra compiler workers. Run lightweight
mock regressions with `bash tools/ci/test_run_linux_qemu_matrix.sh`.

## Product profiles and runtime names

Use `--profile full` or `--profile ros` for release evidence. The default
`--profile auto` recognizes a complete Full command surface or the deliberately
reduced ROS surface; unfamiliar/older binaries retain the runner's legacy
capability-driven auto selection.

A resolved Full or ROS profile is fail-closed: its process exits successfully
only when there are no FAIL or SKIP rows and at least one PASS row. A selection
that is entirely SKIP/N-A, or that mixes a passing subset with unsupported
required coverage, therefore cannot make CI green. Deliberately source-pruned
ROS modes remain N-A. Legacy-profile invocations retain their earlier
exit-status behavior.

The canonical Full product modes identify their final selectors exactly:

- `int-full`: `-Rcm full -Rcc int`
- `int-lazy`: `-Rcm lazy -Rcc int`
- `llvm-full`: `-Rcm full -Rcc jit`
- `llvm-lazy`: `-Rcm lazy -Rcc jit`
- `tiered`: `-Rtiered`

Full `--modes auto` selects exactly those five modes. ROS auto selects only
`int-full` (`-Rint`) and `llvm-full` (`-Raot`); int-lazy, LLVM-lazy, tiered and
their diagnostic variants are recorded as `N-A`. They are never launched and
`--allow-unsupported` does not change N-A into SKIP or PASS. The reduced ROS
command surface must omit both deleted custom selectors, the lazy/tiered
selectors, and both tiered-disable diagnostic selectors. Even one residual
custom or tiered-disable option prevents resolution as the ROS product profile.

The legacy `int`, `aot`, `full`, and `lazy` names remain available with their
original arguments. In particular, Full `int` remains `-Rint`, whose compile
mode is automatic; it must not be reported as int-full. Canonical selections
reject runtime-selector values supplied through `--uwvm-arg`, since a later
selector could otherwise make the result label false. Metadata records the
requested/resolved profile and the exact argument source for every selected
mode.

Each run rebuilds every Wasm fixture; an older output is never reused solely
because its mtime is newer. `metadata.tsv` records UTC bounds plus SHA-256 for
the runner, uwvm, and wat2wasm, as well as the wat2wasm version and command
template. `fixture-manifest.tsv` records the input/origin WAT and generated Wasm
SHA-256 for every case, and its own digest is stored in metadata. Preserve both
TSV files and the referenced logs when publishing evidence.

## Tier-2 evidence boundary

`tiered_full_ready_oob` checks **T2 compilation/publication readiness plus the
expected final trap and logical stack**, not execution of a T2 entry. Its fixed
hot-call loop is not a synchronization handshake, and the same final trap can
occur in T0 or T1. `tiered-full-request` / `tiered-full-ready` are compiler events;
the current runtime exports no T2-specific executed-entry witness.

The case's PASS detail therefore includes `t2_publication=ready`,
`t2_entry_execution=unverified` and `t2_unwind_coverage=unverified`. The same
evidence limit is recorded in metadata. Do not count this PASS as T2 execution
or T2 unwind coverage. Missing readiness or an incorrect trap still fails.

The readiness case requires `--compile-threads >= 2`; smaller budgets are
preserved and the case is SKIP. It uses `instruction`, or `auto` only when the
probe resolves it to `instruction`. Native unwind disables background T2 in the
current runtime, so a matrix with no compatible selected policy gets an explicit
SKIP rather than a purported T2 test. Native-unwind correctness is tested by
other applicable matrix cases, without implying T2 coverage.

TSV retains its existing 20 columns. Executed/applicable cases use
PASS/FAIL/SKIP; deliberately deleted ROS modes use N-A. These labels describe
each case's documented scope, not complete production certification. Metadata
also records result counts and the release-qualification decision.
