# Current-source GC/EH measurement preparation

This is a new source-only plan after the private-EH publication V3 freeze.
No current private-EH timing, general-GC throughput or industry ranking is
qualified. The publication cold controls come first. Historical r9/r10 counts
and VTune samples retain their original source, boot and cgroup identities.
The new KVM scope needs fresh actual admission and hardware receipts; old
scope results do not certify it.

## Workloads and build cells

Keep the original 512M immutable compact ring and 8M-call/500K-catch EH
fixtures as historical continuity controls. The frozen r11 two-header ABBA
wrapper accepts its exact compact workload and source difference; it cannot
be reused to attribute later metadata, mutable setters or EH changes to r11.

Use all four general-GC families and both allocation/mutation phases from
[GENERAL_GC_FAMILY_PLAN_20261002.md](GENERAL_GC_FAMILY_PLAN_20261002.md), first
65,536 iterations cold, then up to the reviewed 2,000,000 bound after actual
collection coverage and completion. Keep the second reference-array table:
2,048 table slots are not the other families' 1,024-slot root snapshot.
Actual allocation/collection/reclaimed counters and finalized lowering are
needed; source formulas are not executed helper counts or collector latency.

Bind the same complete actual source to separate RT/consumer builds. Start
with default macro values, then select one independently qualified candidate.
The six-macro experiment is another cell, never the default or a substitute
for mutable/reference performance. `PRECISE_GC_TRACE_METADATA`,
`NUMERIC_STRUCT_SET32`, `NATIVE_EH_LEAF_OBSERVER` and `NATIVE_EH_PRIVATE_LEAF`
are independent exact-1 gates. No single-candidate comparison silently enables
the other candidates or reuses a different RT object. Preserve all compiler
argv, response-file bytes/overrides, ABI, policy and dependency closures.
Current LLVM full only is the first timed cell; mode/platform qualification
and interpreter assembly are separate cold requirements.

`generate_native_eh_leaf_benchmark.py` prepares a new actual Core 3 `try_table`
workload. One exported numeric leaf takes an i32 and either returns the next
LCG state or throws that same value; exactly one in sixteen calls throws.
The caller's first consuming handler is non-reference. Its observable checksum
and catch count are exported, and its run function checks both before returning.
The leaf stays publicly exported. A separate valid bad-checksum variant must
trap in every engine; it is an executed oracle-strength control, not invalid
Wasm. Start at 65,536 calls/4,096 catches, then 8M/500K after actual cold success.
Official tools encode once, validate, print and re-encode; original `.wasm`
bytes and actual hash are supplied unchanged to both products and every VM.

The new [native owning-source harness](../../test/0017.runtime/llvm_native_eh_private_leaf_benchmark.cc)
uses real CLI parse/init and full runtime entry, with the same binary selecting
public/private via the initially false privileged source request. Each fresh
process performs exactly one run. A private row requires the actual committed
binding and one clone after that run; declined specialization/fallback is a
failed private row, not a timing pass. Ordinary product CLI launch alone never
enables that initially false request. RT and harness use aligned observer and
private gates; no forced-CFI-failure seam belongs in this benchmark build.

The harness emits independently observable returned checksum and actual
binding after its timer. The internal `invocation-total-ns` interval includes
fused validation, JIT compilation and the runtime entry wrapper. Source parsing
and initialization are outside it. It does not measure pure throw/catch latency
or a Wasm-only ROI. External process wall/CPU and pure counts include those
earlier operations, that one invocation and teardown. Do not compare that
internal interval with another VM's complete process time as equivalent scopes.

## Three separate evidence families

1. **No profiler:** fresh-process wall, wait4 user/system CPU, max RSS, the
   harness's declared internal interval or actual CLI guest time, self-check,
   source/ELF closure and raw P0 frequency distribution. Keep ABBA with two
   opposite-direction pairs per product/cell. Report P05/median/P95, sample
   count, throttle counters and host P0/SMT activity over aligned intervals.
   A >10% median-frequency mismatch or insufficient observations does not
   qualify a paired effect. Unknown host activity is unknown, not quiet.
2. **Pure hardware:** separate sessions, P0 single guest TID, actual cpu_core
   type/config group 0x3c/0xc0, no sampling, software events or inheritance.
   Preserve the actual enable ACK before guest GO, returned leader/member FDs,
   member group FD, original owned PIDFD and post-retirement SIGINT, exact raw
   count/enabled/running and JSON cross-check. Require enabled==running for the
   first minimal sample. Whole-guest counts include initialization/JIT/one run;
   retain original counts, instructions/run, cycles/run and CPI/IPC separately.
3. **VTune hardware:** another session using actual installed Hotspots/uarch
   help and successful current-scope hardware permission. Retain exact argv,
   CPU mask 0, actual event domain, MUX/running ratio, sample stack size/skipped
   frames, collector mode and unknown JIT attribution. The old uarch summary's
   MUX 0.561 is not exact 100% perf counting. Source-bound native lambda labels
   cannot be transferred from r9 to this later runtime. Self/inclusive sampled
   time and profiler wall do not replace either preceding evidence family.

Temperature is observed and retained only. It never rejects admission, peaks
or pairs. Existing historical guards remain immutable, but their old thermal
rejection must not be introduced into this new user-authorized contract.
64GiB memory, zero swap, actual allowed CPU set, PIDFD ownership, output bounds,
deadline and complete source/SDK/DSO closure remain hard requirements. The
keeper alone executes; no compilation, Windows VM, QEMU, other guest or other
profiler overlaps a timing session. No foreign process is killed.

The old frozen host counter and r11 wrapper are not mutated. A future runner
that accepts the new benchmark must review its exact fixture/harness self-check
and roster independently rather than replacing a pinned historical command.
The preparation manifest has `execute_ready: false` and no invented build IDs,
ELF hashes, tool versions or native PASS fields.

## Industry capability and comparison order

Wasmtime is the first same-binary control because its existing v49.0.1 ELF and
CLI were actually inventoried previously. Recheck its actual ELF/version/help
and native cold capabilities in the current scope, and bind its selected
collector explicitly. The version-pinned copying/DRC source study is in
[GC_GENERAL_COLLECTOR_DESIGN_20261002.md](GC_GENERAL_COLLECTOR_DESIGN_20261002.md).
Current [collector API documentation](https://docs.wasmtime.dev/api/wasmtime/enum.Collector.html)
still includes a stale copying-status note, so it cannot overturn successful
actual installed-binary execution or the pinned implementation. Its DRC cycle
limitation means cyclic runs need real reclamation/RSS evidence; a short exit
zero is not equivalent bounded cycle collection. `Null` is not a reclaiming
GC competitor. Use copying first; other collectors stay named independent cells.

For WasmEdge, Wasmer and WAVM, record exact downloaded/existing release ELF,
build flags and complete tool/DSO closure before even a capability probe.
Run the same mutable/reference/cyclic GC and current `try_table` bytes cold;
record unsupported syntax or missing compilation modes without timings. The
[WasmEdge 0.16 C API reference](https://wasmedge.org/docs/embed/c/reference/0.16.x/)
distinguishes EH interpreter support, and its
[GC guide](https://wasmedge.org/docs/zh/start/wasmedge/extensions/gc/)
describes proposal switches; documentation alone does not certify the exact
engine/backend used here. Wasmer's
[official releases](https://github.com/wasmerio/wasmer/releases) likewise do not
replace feature probes. No WASI, component model or source rewrite is added to
make an unsupported engine appear to run identical Core 3 bytes.

Only cold-qualified same-binary engines enter unprofiled ABBA. Keep startup
and in-instance timings separate, same number of guest runs and fixed heaps
when actually supported. Report each collector/backend and median/range rather
than announcing a strongest VM from a one-pair or profiled sample. Pure counts
and VTune diagnosis can then explain a measured regression; they do not rescue
an unmatched or unsupported timing row.

Java/GraalVM, JavaScript/V8 and C# are a later, separately labeled algorithm
comparison. They require independently checked payloads, object/root/edge counts,
cycle restoration, actual allocations, collector/heap settings and observable
readback, plus warmup/JIT and execution envelopes. They cannot execute these
same Wasm bytes as a native-language workload, so such results must not be
merged into a same-binary Wasm ranking. An equivalent graph's whole runtime
also cannot be called pure collector latency. No new language timing or
download is performed by this source preparation.
