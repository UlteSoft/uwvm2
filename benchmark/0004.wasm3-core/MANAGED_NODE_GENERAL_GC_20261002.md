# Managed Node general GC candidate, 2026-10-02

Source only. GeneralGc.mjs mirrors the four Wasm/Java/C# families and all eight
allocate/mutate loops. No local Node parser, VM, compilation, SSH or timing ran.
The preserved Java/C# ports, legacy Box-ring JS and original managed runner
remain unchanged. Pure Python checks validate scalar receipts and launch
contracts, not JavaScript execution or allocation survival.

| Family | Logical group and observable operations |
| --- | --- |
| mutable-struct | one mutable32-bit Box; publish, set/read state+i |
| reference-cycle | two value/reference Nodes A↔B; replace self-edge, read, restore cycle |
| numeric-array | eight mutable uint32 cells; selected/following stores and readback |
| reference-array | cyclic A/B plus eight references, separately rooted A; real reference stores/readback |

ROOTS is an exported1024-element module array and is published on globalThis.
Reference-array also publishes1024 NODE_ROOTS; final roots sum all2048 slots.
The LCG seed123456789, multiply1664525/add1013904223, salt0xa5c31f27, WIDTH8,
modulo2^32 and displaced-root/readback/checksum mathematics match
`generate_general_gc.py`. Every warmup and measured Run checks independent
step/root/state/return values. Source escape is necessary but does not prove
that V8 retained every allocation.

Numeric arrays use Uint32Array(8), which also manages an ArrayBuffer and backing
storage. JS classes/arrays, object headers, hidden classes, compressed refs and
number representation differ physically from Wasm. `planned_syntax_allocations`
is the source constructor count; it is not a measured physical object count.
The source outputs unknown GC counts/reclaimed objects as explicit null.

The [official Node index](https://nodejs.org/dist/index.json) candidate is
v26.10.0 with V8 base14.6.202.34. The asset digest and installed executable remain
unbound until the sole Linux keeper records the actual official archive/checksum,
ELF, SDK tree and DSOs. The fixed VM flags are `--max-old-space-size=1024` and
`--max-semi-space-size=16`; [Node CLI documentation](https://nodejs.org/api/cli.html)
defines their requested spaces. This is not an equal physical heap capacity to
Java/.NET/Wasm; the port records actual V8 heap_size_limit independently.

No `--single-threaded`, JIT/escape-analysis disable, forced collection or
concurrent-worker suppression is added. All actual inherited NODE_/NPM_CONFIG_/
V8_/UV_THREADPOOL_SIZE settings, other managed-runtime knobs and loader knobs
are captured privately then removed. NODE_OPTIONS is empty, compile cache is
disabled, capture/preload/audit are absent, the reviewed actual loader path is
restored, and PYTHONDONTWRITEBYTECODE=1 is explicit. Effective stopped/loaded
child environment remains an actual observation; missing observations remain
unknown. Every observed VM/compiler/GC TID must retain UID1000/P0/exact cgroup.
The existing multi-TID guard/SDK tree functions are byte-identical to the frozen
managed parent. Product single-TID HW counters are not widened to cover Node.

## Minimal keeper preparation

These are inner commands admitted through the existing reviewed guardian within
64GiB/swap0. No compiler/profiler/timing may overlap P0 samples. Variables without
actual fixed pins do not form a runnable plan.

First guarded actual controls, with inherited relevant settings stripped:

    "$NODE_ACTUAL" --eval 'console.log(JSON.stringify({node:process.version,v8:process.versions.v8,platform:process.platform,arch:process.arch}))'
    "$NODE_ACTUAL" --check "$GC_SOURCE/managed_general/node/GeneralGc.mjs"
    "$NODE_ACTUAL" --v8-options

The exact version receipt is `{argv,returncode,version}`; version contains
node/v8/platform/arch. The syntax receipt is `{argv,returncode,node_sha256,
source_sha256}`. Both are actual controls, not fabricated by the producer.
Bindings schema `uwvm-node-general-artifacts-v1` requires canonical absolute
`node`, `version_receipt`, `syntax_receipt` path/SHA pairs. The runtime closure
schema `uwvm-node-general-runtime-closure-v1` requires actual `sdk_roots.node`
path/tree_sha256, `loader_environment.LD_LIBRARY_PATH`, and nonempty actual
loader/DSO `files` pins. The producer only binds existing files and computes
scalar expectations; it launches nothing.

    python3 -B "$GC_SOURCE/prepare_node_general_plan.py" --bindings "$GC_OUT/actual-node-artifacts.json" --out "$GC_OUT/node-cold-plan.json" --mode cold
    python3 -B "$GC_SOURCE/run_node_general_measurement.py" --plan "$GC_OUT/node-cold-plan.json" --admission "$FRESH_ADMISSION" --sdk-closure "$GC_OUT/actual-node-closure.json" --out "$FRESH_COLD_EVIDENCE" --execute

Cold contains all eight cells at65536, warmup rounds0. After complete actual
cold and closure, the initial warm plan is four representative cells:
reference-array/allocate and mutable-struct/mutate at1M/2M, warmup8×250000.
Each Run resets roots/setup; carried heap/JIT state is intentionally retained.
Warmup count does not prove stable tiering. No cold65536 result is a2M ranking.

    python3 -B "$GC_SOURCE/prepare_node_general_plan.py" --bindings "$GC_OUT/actual-node-artifacts.json" --out "$GC_OUT/node-warm-plan.json" --mode warm
    python3 -B "$GC_SOURCE/run_node_general_measurement.py" --plan "$GC_OUT/node-warm-plan.json" --admission "$FRESH_ADMISSION" --sdk-closure "$GC_OUT/actual-node-closure.json" --out "$FRESH_WARM_EVIDENCE" --execute

Internal execution_ns measures Run setup/LCG/field operations/allocation/final
roots and excludes warmup/output. Parent wall/user/sys/RSS covers the complete
process and all workers, including startup/JIT/warmup/output/stopped bootstrap.
Neither is collector-only latency. Temperature is observation only; sysfs
frequency snapshots are not ROI effective GHz. Missing exec/TID/frequency
coverage stays unqualified. No PMU, VTune or industry rank is inferred.

## Separate untimed allocation and GC evidence

A separate `--gc-telemetry` plan brackets Run with marker lines outside the timer
and records memory snapshots. [Node heap statistics](https://nodejs.org/api/v8.html#v8getheapstatistics)
and memoryUsage snapshots are not allocated-byte counters. The preserved timed
plan has no marker/snapshot/GC tracing flags.

Installed `--v8-options` must confirm `--trace-gc-nvp`, `--trace-opt`,
`--trace-deopt` before an independent untimed diagnostic command adds those
flags to the same pinned source/scalar args and optional --gc-telemetry.
The [pinned V8 flags source](https://raw.githubusercontent.com/nodejs/node/v26.10.0/deps/v8/src/flags/flag-definitions.h)
provides those GC/tiering traces. Such output and tracer overhead are not timed
results, and the fixed timed runner deliberately rejects extra VM flags.
Record actual GC event kinds/reasons and positive allocated-byte fields when
available, actual loop tier/deoptimizations, and actual allocation/publication
code. Inspect the named four loops or real generated PCs, not unrelated names.
A zero collection cannot be renamed positive reclamation; live roots/physical
allocation counts remain unknown until actual evidence. The untimed diagnostics
may support allocation survival but do not prove a specific reclamation count.

The identical Wasm bytes use actual pinned Wasmtime copying. DRC cannot qualify
cycle reclamation. Current official WAVM lacks these Core3 GC/EH operators;
unsupported is not a performance result. Java/Graal/.NET and new Node compare
the logical program only, with collector/configuration/physical differences and
cold whole-process versus warmed internal Run boundaries retained.
