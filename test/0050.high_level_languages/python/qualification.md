The real CPython3.14.8 producer and native/Wasmtime references passed on 2026-10-18: six positive cases across three programs and two engines (36), plus five invalid-argument cases on each engine (10), total46/46. All extraction, bytecode production, builds and reference execution ran in the existing SSH Linux64GiB cgroup. UWVM ordinary/DBG and formal benchmark/VTune results belong to the parent matrix and are not claimed by this producer report.

Local evidence: [qualification receipt](evidence/producer-r1/qualification-r1.json), [build receipt](evidence/producer-r1/work-r4/build-receipt.json), [references](evidence/producer-r1/references-r1/report.json). Raw command/reference logs remain in the Linux arena. Official CPython source SHA matches; SDK24's release asset has no published digest, so its observed SHA/metadata/reverification are recorded without calling that SHA an independently published official checksum. Failed transfer/parser attempts remain separate; successful retired commands were reused on resume.

Actual artifact prefix: `/tmp/high-level-20261017/python/work-r4/artifacts/`.

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| `python-semantics.wasm` | 27,897,106 | `5c20d7d84842a6e97b2f445f6b38179ec7e0211bf73f0c091f0a7b55f0fe928b` |
| `python-timed.wasm` | 27,897,546 | `c0376d1694688de46d6c8c34917c9915186e81b9fa7ef0d7c3622c980a3a267d` |
| `python-profile.wasm` | 27,898,254 | `0d0c5ce5bf911c4e8e44b760322e29d6e976e44c1a0d9b45c6a7d76ff7ad7e1d` |

Matching `.native` applications and built native CPython3.14.8 are retained. Each Wasm has 8,841 defined functions, memory/_start exports, and 42 WASI Preview1 imports. [Module summary](evidence/producer-r1/manifests-r2/modules-summary.json) preserves exact signatures, exports, DWARF/frozen source/header records and debugger anchors; the full name list stays in remote `manifests-r2/modules.json`.

Real Python classes, lists, dictionaries, Unicode, chained exceptions/finally, bytes/memoryview ownership, cycle collection, argv and environment precede the 64-slot kernel. Pinned native source and an independent scalar oracle agree. Default/1,000 rounds checksum262020; 20,000 rounds checksum264715. Exact normal stable stdout:

```text
python classes=42 arrays=55 dicts=6 unicode=4 utf8=12 exceptions=3 bytes=120 memory=369 gc=1 args=0 env=0
Iterations: 1000
Checksum: 262020
```

Timed outputs append one positive integer `Kernel ns: N` line. Monotonic timing excludes semantics/setup, 100-round warmup, clock warmup and both profile gates. Twelve real profile runs verify READY/G/DONE/D blocking, process liveness and no premature output; stderr markers are exact. These times are calibration/reference data, not formal performance ratios. Invalid0/-1/100000001/x/huge integer cases return2 with empty stdout and exact `python invalid iterations\n` stderr. Ordinary positive stderr is not separately required empty by the helper; positive stable stdout, negative stderr and profile markers are strict.

Producer/reference commands retain authenticated pidfd lifecycle, wait status, retirement/closed records. Reference memory events stayed unchanged; peak observed summed owned build RSS1,022,058,496bytes. The same64GiB/swap0 cgroup remained unchanged. Immutable pre-reference artifact records retain producer-time `qualification: not yet executed`; the later hash-bound reference report supplies actual execution status.

[Functional manifest](evidence/producer-r1/manifests-r2/cases.json) has6positive/5negative rows; only rounds-1000 enables DBG. Negatives have skip_debug=true, exit2, empty stdout and diagnostic. Two explicit-env rows use noinherit plus key/value load_args; paths/preopens/entry overrides are absent. It directly fits run_matrix.py; that helper's stderr predicate is substring-based, unlike exact negative/gated reference checks.

[Ordinary timed](evidence/producer-r1/manifests-r2/perf-timed-cases.json) and [gated profile](evidence/producer-r1/manifests-r2/perf-profile-cases.json) bind separate real Wasm/source SHA, complete stable checks, 20,000 rounds, positive monotonic nanoseconds, and producer/oracle/module receipts. Later calibrated counts need new oracle evidence without overwriting these references.

Minimal frozen startup is not full stdlib. The stock command, actual Lib source and WASI sysconfig are retained for a separate filesystem-bundle route, not covered by zero-preopen qualification. The source inventory counts `.py` files, not all package data. [SDK-reuse.md](SDK-reuse.md) gives exact paths, static inittab/relink instructions and that separate file contract. [ROS stdlib notes](ros-stdlib-source-notes.md) are static review, not PASS evidence. DWARF is CPython C; no Python-source/locals debugger bridge or ROS transport is claimed here.

The distinct full71-builtin Oz producer and its11 Wasmtime references are described in [debug-producer-notes.md](debug-producer-notes.md). Its exact executed producer SHA `f3b10b16d5df0972b925b5d612c74f2fa6358f5633ad56b52ba6ebffd746bf03`, direct helpers, real reference helper, historical failed sources and original bytecode header are now supplied in [debug-producer/evidence-sources](debug-producer/evidence-sources/capture-manifest.json). [REBUILD.md](debug-producer/REBUILD.md) gives actual argument syntax for new O2/native/base and Oz directories, with the existing-cgroup/owned-process wrapper and complete external pins. Those fresh-directory instructions are static documentation and were not executed during source capture; actual historical FullOz completion used `--resume-built`. Original reports/hashes remain unchanged. Oz's parent-owned UWVM DBG qualification must be read separately and does not qualify O2 performance Wasm DBG.
