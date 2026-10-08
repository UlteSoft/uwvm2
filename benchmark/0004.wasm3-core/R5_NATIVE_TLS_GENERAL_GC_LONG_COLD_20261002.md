# Current R5 native TLS long General GC cold binding — 2026-10-02

Status: implemented source recipe; Linux keeper execution pending. This does not
alter the old S6e runner, corpus, source receipts, or current 65536-step cold
records. The current R5 ordinary product has actual CLI8 and GC36 cold evidence.
Its 1M/2M product cold results are not yet available.

The new fixed recipe contains 34 commands: binding/source before, 32 product
executions, binding/source after. It uses all four families (mutable struct,
reference cycle, numeric array, reference array), allocate/mutate, 1M/2M steps,
and instruction/unwind policies. Source and actual main/RT1/host3 build identity
stay identical across both policies.

## Actual product and inherited evidence

The source is the frozen current ordinary R5 production baseline, external source
ID sha256:a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a.
The product main is 94,107,376 bytes, SHA-256
4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632.
Every TU uses native TLS. All six GC experiments, SET32, collection membership
and the new array authentication candidate are omitted. CAPTURE=1 support was
compiled, but the runtime capture environment variable is stripped before
launch and must be absent in the supervisor's actual stopped-child witness.

CLI8 includes one fresh main plus this-task actual RT1 and host3 objects.
The recipe reuses the exact reviewed R5 closure helper's product function, not
its old cold main, and rechecks every actual component dependency, object,
tool, SDK archive and library pin before/after. It does not claim all stages
fresh or actual loaded DSO maps qualified. Build closure/schema stays original.

The unchanged approved supervisor has SHA-256
3a05cc7fc0d019b929b10e4d7518d3f8373ddbffaa2124a947a8ec9999fe07a9.
The reused R5 closure helper has SHA-256
30c404354f823ad11fe80d409b10d993227edc0b4b0ea55566869ab9d453ba7f.
It is loaded only through its definitions before the exact original main
boundary, after whole-file SHA validation. Python -O is rejected because its
reviewed checks use assertions.

The sixteen Wasm files are reused from the original General GC long corpus.
Only its official binary validate, exact WAT roundtrip, Wasmtime 49.0.1 copying
run checksum and _start exit records qualify the fixture bytes. Those four
real receipt/log types are individually checked, including their actual argv,
resource limits and retired PIDFD ownership. Old S6e product outcomes are
never consulted as R5 qualification.

## Keeper preparation and sole execution

The script is source-only on the workstation. Copy its reviewed bytes to a new
keeper source path, then run prepare under the existing 64GiB sandbox. It creates
a fresh build directory; an existing directory or evidence path is rejected.

    python3 prepare_general_gc_R5_nativeTLS_long_cold.py prepare

It creates the fixed remote directory:

    /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/general-gc-R5-nativeTLS-long-cold-20261002-r1

Use the existing trusted keeper supervisor launch/admission procedure. The
actual supervisor argv is:

    python3 /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/general-gc-R5-nativeTLS-long-cold-20261002-r1/supervisor.py --out /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/evidence/general-gc-R5-nativeTLS-long-cold-20261002-r1 --command-json /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/general-gc-R5-nativeTLS-long-cold-20261002-r1/commands.json --deadline-seconds 900

This is functional cold work on E16-31, not a P0 performance trial. The original
supervisor enforces 64GiB memory, swap0, owned RSS <=56GiB, birth/UID/actual
parent/CG/CPU ownership and retirement. No compiler, VM or other measurement
runs concurrently. The new recipe introduces no new supervisory permission,
no sudo/global setting, no software counter fallback and no temperature gate.

## Actual semantic and collection gates

Every product argv explicitly uses full LLVM JIT, pb-o3, policy instruction or
unwind, exception auto, cache disable, compile threads zero, GC/reference-types/
function-references, log verbose, -Rclog err and exactly one original Wasm path.
The before/after recipe command JSON and supervisor are pinned unchanged.

Allocate requires the manifest's exact planned allocation count, roots_requested
1, disabled 0, positive attempts/collections/reclaimed and reason 0. Mutation
requires exact initial 1024/2048/1024/3072 allocations, zero attempts/collections/
reclaimed, roots_requested 1, disabled 0, reason 2 (phase_pending). Mutation
qualifies field/lookup only; collector_qualified remains false.

Exit zero alone is insufficient. Each row needs the actual retired receipt,
absent capture variable, real native/owning/no-fallback/cache-disabled witness
and both real internal time lines. The generated Wasm _start checksums read the
root ring and reject incorrect execution. The metadata records planned syntax
operations; it does not invent a collection count or reclaimed count.

The internal Total WASM execution timer excludes startup/JIT and includes all
Wasm setup, field access, allocation/collection and checksums. Total process
timer is separate. Neither is a pure collector ROI. This cold round's wall
observations are not a frequency-matched performance result.

## Subsequent measurement contract

After the actual summary passes, a separate narrow R5 binding may use exactly
the same bytes and nativeTLS/source/profile/actual transitive closure for:

1. Unprofiled P0 parent wait4 whole-process wall/user/sys/RSS and internal Wasm
   timer, preserving actual executable/environment/TID observations. Wasmtime
   whole-process accounting may include multiple individually checked TIDs.
2. Product P0 single-TID pure hardware grouped cpu_core raw 0x3c/0xc0,
   ACK-before-GO and original-owned-SIGINT-after-retirement. Counts cover
   startup/JIT plus execution, not the internal Wasm ROI. Actual FD group,
   time_enabled/time_running and raw/JSON crosschecks are required.
3. Separate VTune hardware Hotspots/uarch samples with actual installed CLI,
   target PID/TID/P0 report filtering and actual MUX/stack limitations. The
   previous target rejected cpu-mask=0; real report filter is cpuid=cpu_0.
   Anonymous JIT mapping remains unknown without actual PC/source binding.

Temperature is raw observation only. Frequency distributions, PMU exposure and
SMT/noise evidence retain their own qualifications. A missing short-row
frequency or actual-exec observation remains unknown/unqualified. Different
measurement families and ROI boundaries do not substitute for one another or
justify industry ranking.

No R5 long cold, P0 timing, hardware count or VTune result is declared by this
source-only recipe. The array reference authentication candidate stays in a
separate frozen source and is not enabled here.
