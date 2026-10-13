# Same-platform Wasmtime/WAVM reference commands

This is a source-only command audit, not another measurement framework. Ordinary current R3c EH functionality is closed; its P0 adapter is separately frozen. ROS R3f and GC A/B/C have separate actual source/build/cold records. They must not inherit historical S6e/non-native-TLS or failed R3d qualification. Only the sole Linux keeper executes these commands, serially inside the current 64-GiB/swap0 scope, after compilation/VM/profiling retires. E-cores build tools; P0 measures guests. Temperature is observation only.

## Actual tool inventory and required receipts

Let `B` be `/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924`.

| Tool | Actual Linux path / recorded ELF SHA-256 | Current qualification |
| --- | --- | --- |
| Wasmtime 49.0.1 | `B/tools/wasmtime49-unpacked/wasmtime-v49.0.1-x86_64-linux/wasmtime`; `c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94` | Historical actual same-byte copying oracles exist. Fresh current-source comparison needs its own admission, argv/environment/cold and complete result closure. |
| Wasmtime 42 | No actual path/version/ELF/help receipt supplied | Not an installed qualified reference. Do not invent a path or copying capability. |
| WAVM inventory | `B/tools/wavm-nightly-2026-04-05/bin/wavm`; 872592 bytes, `daf29ef8ebbd9eaa20a13115205b56fafd4b8cfd77b38333c647bb12851ecd23` | File inventory only. Actual version, official archive provenance, shared-library hashes/build IDs, runtime feature support and cold execution remain pending. |

WAVM's neighboring `archive.tar.gz` is 224651941 bytes and `lib/libWAVM.so.0.0.0` is approximately 633710024 bytes; their digests are not supplied here. The directory name alone does not prove official commit identity. Obtain the actual archive/member SHA, version output, ELF interpreter/NEEDED closure and exact loaded DSO paths before using it as an official reference. Do not replace or modify the user's local `/Users/liyinan/Documents/MacroModel/src/WAVM` research fork.

The official candidate is commit `4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09` (`nightly/2026-04-05`). Its command dispatcher uses `wavm version`. Its run parser accepts `--abi=bare`, despite help listing `none`; the candidate command must use `bare`. It also supports cache disabling and explicit function selection. These are source findings, not evidence that the existing ELF has this exact provenance. [Pinned version dispatcher](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Programs/wavm/wavm.cpp), [pinned run parser](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Programs/wavm/wavm-run.cpp).

Initial keeper-only tool receipts, with real output/exit/argv/ELF/DSO/environment pins:

```text
ACTUAL_WASMTIME49 --version
ACTUAL_WASMTIME49 run --help
ACTUAL_WASMTIME49 run -C help
ACTUAL_WASMTIME49 run -W help
ACTUAL_WAVM version
ACTUAL_WAVM help run
```

Only actual installed-help-supported knobs advance to cold execution. Unexpected help behavior is a real retained receipt, not proof that a feature works. Capture inherited runtime/profiler/cache/loader settings and the cleaned actual child environment. WAVM's library directory may need a different exact loader closure from Wasmtime/uwvm; derive and pin it rather than appending guessed library paths.

## Capability and collector boundaries

The pinned WAVM feature definitions include memory64; Core3 managed GC and modern `try_table`/`throw_ref` are absent. The four mutable/reference/array GC families and the modern EH fixture are therefore excluded from this official candidate's performance comparison unless a different actual supported product is separately proved. Unsupported is neither a slow score nor PASS. Its ordinary common instructions and memory64 remain useful comparisons. [Pinned feature definitions](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Include/WAVM/IR/FeatureSpec.h).

Wasmtime 49's pinned CLI source accepts copying, DRC and null collector selections. Installed help and exact-byte cold still decide what the ELF supports. Version 42's pinned configuration has Auto/DRC/Null, without Copying; do not apply current floating 50-dev documentation to it. [49.0.1 CLI configuration](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/cli-flags/src/lib.rs), [42.0.0 collector configuration](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v42.0.0/crates/wasmtime/src/config.rs).

Copying is the current reclamation comparison for cyclic workloads. DRC cannot reclaim cycles and null never reclaims; their acyclic or non-reclaiming diagnostics are distinct, explicitly labeled capabilities. Do not turn exit0 into a collection/reclaimed counter. Actual collector evidence available from instrumentation or retained source-bound diagnostics and memory growth stays separate from throughput. [Pinned 49 copying implementation](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs), [pinned 49 DRC implementation](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs).

## Exact-byte cold and first paired commands

Use the already official-valid fixed `generate_general_gc.py` v2 four-family manifests and byte-identical `.wasm` files. Cold both 1M/2M before comparing current uwvm products. Preserve the manifest's root groups, WIDTH8, mod32 LCG, step/root/state/return checksums and `_start` self-check. `run` invokes the checksum-returning export; `_start` is the identical-byte self-check entry. No legacy immutable Box-ring data is substituted.

Wasmtime GC commands below retain the already used copying flag shape; explicit Cranelift may be added only after installed-help confirmation and a new complete cold record. Default/explicit-copying/default-compiler configurations are recorded independently; no silent flag changes during a pair.

```text
taskset -c 0 ACTUAL_WASMTIME49 run -C cache=n,collector=copying
  -W all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y EXACT_GENERAL_WASM

taskset -c 0 ACTUAL_WASMTIME49 run -C cache=n,collector=copying
  -W all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y
  --invoke run EXACT_GENERAL_WASM
```

Modern EH cold/control uses the same three exact current R3c EH files, with installed-supported exceptions enabled for `eh_normal`/`eh_throws`. Confirm `_start` self-check and native EH support before timing. Wasmtime CLI cold invocation and whole-process startup/JIT are not uwvm's internal Wasm timer.

```text
taskset -c 0 ACTUAL_WASMTIME49 run -C cache=n -W exceptions=y EXACT_EH_WASM
```

Memory64's historical exact fixture directory is `B/evidence/bench-core3-c7f9-43a37-memory64-20260928/memory64/fixtures`:

| Fixture | Exact SHA-256 |
| --- | --- |
| `memory64-random-store-5000000.wasm` | `5ed7090a0cbafe866be3eae0bbfc76ceeed6b1cb607dc0bc6392f4a829269fd9` |
| `memory64-random-store-50000000.wasm` | `322641f90c7b80225e942031988918da63e925e725a72dd2dd1f6c4b28cb36cc` |

First actual official validate, tool cold and current uwvm source-bound cold. Then the existing multi-TID reference/plain wait4 protocol may admit the reference tools with each actual TID's UID/P0/exact-cgroup proof; do not insert an unknown WAVM/Wasmtime process into the strict single-TID product counting protocol.

```text
taskset -c 0 ACTUAL_WASMTIME49 run -C cache=n -W memory64=y EXACT_MEMORY64_WASM
taskset -c 0 ACTUAL_WAVM run --nocache --abi=bare --function=_start --enable memory64 EXACT_MEMORY64_WASM
```

Initial comparison is whole-process GO-to-reap wall and actual wait4 user/sys/RSS, on the same bytes and platform with cache disabled and alternating order. Record compilation/startup contribution independently. Keep uwvm's actual internal Wasm execution timer separately; WAVM's own execution timer begins after module load/compile/instantiation and includes host execution glue, and its millisecond print precision differs. No Wasmtime-only-CLI guest ROI is invented. Do not place these unlike internal timers in one throughput column.

Frequency snapshots remain snapshots. Effective ROI GHz requires actual aligned VTune target PID/TID/P0 reports or independently supported hardware reference cycles, not inferred scaling_cur_freq. Pure grouped counters and VTune sampled/multiplexed events stay separate. Capture JIT assembly only in untimed runs; measured children must prove capture absent.

## If the existing WAVM cannot be officially closed

Only then prepare a separate clean official source at the pinned commit; never overwrite the research fork. Pin Git/submodule bytes, actual CMake/Ninja/compiler tools, LLVM headers/static libraries, CMakeCache, compile/link commands, MD/RSP, ELF and DSO closure. The official build documentation requires LLVM20 or newer and recommends the patched WAVM-LLVM21; this does not automatically qualify the bundled LLVM23. [Pinned build instructions](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Doc/Building.md).

The following are inner E-core commands for the keeper's current resource-limited build supervisor, with real fixed absolute paths and a fresh directory required before launch:

```text
taskset -c 16-31 ACTUAL_CMAKE -S EXACT_CLEAN_WAVM_SOURCE -B FRESH_WAVM_BUILD -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=EXACT_QUALIFIED_LLVM_CMAKE
  -DCMAKE_C_COMPILER=EXACT_CLANG -DCMAKE_CXX_COMPILER=EXACT_CLANGXX
taskset -c 16-31 ACTUAL_CMAKE --build FRESH_WAVM_BUILD --target wavm --parallel 16
```

Absent LLVM/API/DSO closure is pending, not a failed runtime score. No administrator command, software-counter substitute, official tool build, download or native execution was performed by this source audit.
