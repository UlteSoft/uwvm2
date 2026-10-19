# Java -> Wasm fixtures

`src/main/java/uwvm/hll/Semantics.java` is compiled from Java source to JVM
bytecode, then to real WebAssembly by **TeaVM 0.13.1**. It does not embed a JVM
or hand-write the equivalent WebAssembly.

The fixture checks interface/virtual dispatch, inheritance/casts, multidimensional
arrays, array copying and zero initialization, implicit null/bounds exceptions,
Java catch/finally semantics,
UTF-16 supplementary characters, strings, 64-bit arithmetic, integer shifts,
cyclic object allocation, and preservation of a live cyclic object. It allocates
8192 arrays of 257 integers with an explicit `System.gc()` request every 1024
iterations. WASI uses a 1 MiB initial / 4 MiB maximum TeaVM heap; the program
requests collection but does not claim to measure reclaimed bytes or collector
pause times. The deterministic calculation gives a useful finite profiling
workload as well as semantic coverage.

## Build on the authorized SSH Linux host

Run all downloads, builds, executions, debugger checks and profiling in the
already existing `uwvm-debug-core3-v48-20261011` container, whose cgroup memory
limit is 64 GiB. `docker exec ... chroot /host` preserves that cgroup and makes
the existing host Java installation and shared libraries available. The
directory used for this run is `/tmp/high-level-20261017/java` inside
that chroot (`/host/tmp/high-level-20261017/java` from the container root).
After a Linux reboot cleared the initial tmpfs cache, the same path was restored
through a symlink to the persistent `/home/macromodel/Documents/uwvm-hll-20261017`
arena. `original-restore-hashes.json` compares the five original module hashes
across that recovery. No new cgroup or memory-limit change is needed.

Copy this fixture directory into the scratch directory, then execute there:

```sh
python3 bootstrap-maven.py /tmp/high-level-20261017/java
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
export MVN=/tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn
bash build.sh -Dmaven.repo.local=/tmp/high-level-20261017/java/m2
```

The bootstrap pins Maven 3.9.11 and verifies its archive SHA-512. The POM pins
TeaVM and lifecycle plugin versions, preserves the standard Wasm function-name
section (`minifying=false`), and enables DWARF and TeaVM debug metadata.
Java 21.0.10 was used for this run, with
Java source/target level 17. The installed Java compiler module lacks `ct.sym`,
so the build deliberately uses source/target instead of `--release`; this does
not certify compatibility with the entire Java 17 API. The source uses only
the small standard-library subset checked here.

Outputs:

| Path below `target/` | Backend | Optimization | Debug metadata |
| --- | --- | --- | --- |
| `wasi/java-semantics.wasm` | WASI Preview 1, linear-memory TeaVM runtime | FULL | enabled |
| `wasi-debug/java-semantics.wasm` | WASI Preview 1, linear-memory TeaVM runtime | SIMPLE | enabled |
| `wasm-gc/java-semantics.wasm` | WebAssembly GC, TeaVM JS host runtime | FULL | enabled |
| `perf/java-perf.wasm` | WASI Preview 1, adjustable array/integer kernel | FULL | enabled |
| `known-gap/java-implicit-arithmetic.wasm` | WASI Preview 1, known implicit-exception gap | FULL | enabled |
| `perf-timed/java-perf-timed.wasm` | WASI Preview 1, monotonic guest kernel timer | FULL | enabled |

Compare stdout byte-for-byte with `expected.stdout`. A native JVM reference can
be run with `java -cp target/classes uwvm.hll.Semantics` in the same cgroup.
The WASI modules have both a Wasm start section (TeaVM heap initialization) and
an exported `_start` (the Java command entry). A command runner must instantiate
the module, including its start section, then invoke `_start`. An exit status
of zero with empty stdout is not a semantic pass.

`uwvm.hll.Perf` accepts one decimal iteration-count argument, between 1 and
1,000,000,000 (default 1,000,000). Calibrate it separately for each execution
backend so that guest execution lasts more than one second and less than 30
seconds. Compare its output with the JVM at the same iteration count. Attach
VTune after the VM has reached guest execution to exclude module compilation
and JIT startup. `perf-expected-1000000.stdout` records the default-count JVM
reference; it is not the expected output for another count.

## Explicit WASI clock authority

The ROS runtime's WASI clock permissions default to denied. Java timed fixtures
that call `System.nanoTime()` require
`--wasip1-global-grant monotonic-clock` before `--run`; enabling a language's
debug adapter does not grant clock authority. The retained clock probe checks
that the same gated module exits with code70 without this permission and
completes with its exact checksum when only this permission is granted.
Do not grant unrelated clocks, random access, filesystem mounts or process
operations to make a timer benchmark start.

`PerfProfile` has an additional test protocol: it prints `UWVM_PROFILE_READY`
to stderr and waits for `G`, measures the kernel, prints `UWVM_PROFILE_DONE`
and waits for `D`. Profilers attach after READY, stop after DONE, then send D
and validate all semantic output. Preloading `GD` is a functional diagnostic;
it is not a valid hardware attach benchmark. Native JVM timings and TeaVM Wasm
timings use the same source calculation but different language-runtime designs.

## Backend and host contract

TeaVM's WASI backend is deliberately pinned to 0.13.1 because TeaVM 0.14 removed
its old linear-memory WebAssembly and WASI backends. This is a compatibility
route for a WASI Preview 1 runtime, not a claim that current TeaVM's primary
GC backend has a WASI ABI. See the [official release notes](https://github.com/konsoletyper/teavm/releases/tag/0.14.0)
and the [0.13.1 target enum](https://github.com/konsoletyper/teavm/blob/0.13.1/tools/core/src/main/java/org/teavm/tooling/TeaVMTargetType.java).

The Wasm GC output requires TeaVM's JavaScript host runtime.
TeaVM's [official loader API](https://teavm.org/docs/wasm-gc-backend/loader.html)
supplies JavaScript host imports. Raw Wasm GC feature support alone does not
supply those imports. A standalone runtime must explicitly implement the
observed contract before this backend can count as a successful execution.
Unsupported imports must be reported as unsupported, never replaced by
no-op stubs to manufacture a pass.
The observed WASI imports are exactly `wasi_snapshot_preview1.fd_write`,
`clock_time_get`, `args_sizes_get`, and `args_get`. The GC module imports 24
functions from `teavm`, `teavmConsole`, `teavmJso`, and `wasm:js-string`; it
also contains legacy `try` exception instructions. Wasmtime 42.0.1 with
`-W all-proposals=y` rejects those legacy exception instructions, and raw UWVM
loading reports the missing `teavm` module dependency. Neither is a passing
execution of this GC backend.

## Observed compiler limitations

The positive semantic fixture tests explicitly thrown Java exceptions and
implicit null/bounds exceptions with TeaVM's strict checks enabled.
`ImplicitArithmetic` separately retains an unguarded integer divide by zero:
the JVM catches `ArithmeticException` and emits `implicit-arithmetic-jvm.stdout`,
whereas TeaVM 0.13.1 WASI emits a Wasm integer-divide-by-zero trap. Wasmtime
42.0.1 reproduces this failure; it is not evidence of an UWVM regression. Do
not report this fixture as a passing Java semantic test. An inlined literal
zero can additionally crash TeaVM's own `BoundCheckInsertion` pass at
`BoundCheckInsertion.java:271`; parsing the zero makes the retained fixture
reach guest execution so the runtime gap can be tested independently.

The pinned producer's `BaseWasmGenerationVisitor` lowers integer/long division
directly to `DIV_SIGNED` and remainder to `REM_SIGNED` (lines 181-188 and
268-318). The already enabled strict option does not insert an arithmetic
exception path. `ExceptionHandling` supplies NPE/CCE/array-bounds helpers but no
arithmetic helper, and `WasmTarget` links those existing helpers at lines
393-398. A complete future producer fix must evaluate both operands once in
Java order, throw a real Java `ArithmeticException` on zero, register its
dependency and route it through the current Java handler/call-site mechanism.
Integer and long `MIN_VALUE / -1` must return `MIN_VALUE` rather than a Wasm
overflow trap; corresponding remainders must return zero. Constant-zero
folding in `BoundCheckInsertion` also needs a guard. This run retains the
original negative fixture and leaves that larger exception-lowering change
as an explicit unsupported producer feature.

The fixture does not cover reflection, class loading, Java threads, native JNI,
filesystem/network APIs, or the entire Java standard library.

## TeaVM producer-side DWARF correction

The unmodified TeaVM 0.13.1 SIMPLE module is retained. Its source breakpoint
probe is rejected as invalid/unsupported debug information. Independent
`llvm-dwarfdump` inspection found a line-address origin inconsistent with the
Code-relative subprogram ranges and a final unterminated line sequence. In
particular, Java main's source line 105 was emitted at `0x124d3`, although its
real Code-payload coordinate is `0x11b70`.

`patches/teavm-0.13.1-wasm-dwarf.patch` corrects the **compiler source**:
standard DWARF line addresses get their own Code-payload origin, separately
from TeaVM's private debug format, and each function closes any open line
sequence. `build_dwarf_fixed.py` verifies the pinned official compiler and
source archive hashes, compiles the two changed upstream classes, repackages
a private compiler JAR, copies the Maven cache into an isolated repository,
and recompiles the original Java source. It does not edit a generated Wasm
binary or its debug sections, and it never modifies the original Maven cache.

Run inside the same authorized Linux cgroup:

```sh
python3 build_dwarf_fixed.py \
  --original-m2 /tmp/high-level-20261017/java/m2 \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --project /tmp/high-level-20261017/java \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages \
  --out /tmp/high-level-20261017/java/dwarf-fixed-toolchain
```

The output is `target/wasi-debug-dwarf-fixed/java-semantics.wasm`.
The build receipt records original/patched source and JAR hashes, commands,
and artifact identities. It requires all non-custom Wasm sections to remain
byte-for-byte identical to the original SIMPLE module. This check and the
Wasmtime output comparison both passed for the producer fix. Independent
`llvm-dwarfdump --verify` exits zero; some pre-existing empty-children DIE
warnings remain, and the unterminated-sequence warning is gone.

`run_source_debug.py` exercises an actual source breakpoint and source step.
The strict invocation below uses the final ordered-name/typed-local variant;
build it first with the optional producer flags described in the next section.

```sh
python3 run_source_debug.py \
  --binary /tmp/high-level-20261017/products/uwvm2/uwvm \
  --wasm /tmp/high-level-20261017/java/target/wasi-debug-dwarf-name-typed-fixed/java-semantics.wasm \
  --source /tmp/high-level-20261017/java/src/main/java/uwvm/hll/Semantics.java \
  --expected-source uwvm/hll/Semantics.java --line 105 \
  --expected-name meth_uh_Semantics_main \
  --expression checksum --expected-value=-2080261270 \
  --expected-after-step-value=-2080261270 --expected-after-step-line 106 \
  --require-values --expected-stdout /tmp/high-level-20261017/java/expected.stdout \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages \
  --out /tmp/high-level-20261017/java/source-debug
```

Use `--ros` for ROS/stable backend selection. The runner uses admitted CPUs
16-31 and seven JIT worker threads until a real `startw` entry stop, then pins
all target threads to CPU 21 before source execution. It verifies the exact
file/line and the original producer function name, requires a fresh source
coordinate after `step source ... over`, and reports locals/print availability
separately. A successful function-name query alone cannot establish source
variables or managed-object support. The FULL artifact omits Java source rows
105/106 due to optimization, so the explicit line-105 probe uses SIMPLE.

## Ordered names and real Java scalar variables

The original module has 661 named functions, with no duplicate indices or empty
names. Its name map nevertheless jumps from index 406 back to imported index 0
at entry 403. The [name-map convention](https://webassembly.github.io/spec/core/appendix/custom.html#name-maps)
requires increasing indices. The standard parser therefore retains names only
before that jump; the Java main function at index 553 loses its displayed name.
`patches/teavm-0.13.1-ordered-names.patch` sorts function names and the outer local
name map by actual function index in the compiler's renderer.

Add `--ordered-names --fixture-output wasi-debug-dwarf-name-fixed` to
`build_dwarf_fixed.py`, with a fresh `--out`. Its receipt compares all 661
original name identities, requires that only their order changed, and still
requires every non-custom module section to be byte-identical. The original
`wasi-debug` and DWARF-only `wasi-debug-dwarf-fixed` outputs remain available.

The producer also emits each local's DWARF index as `i + 1`, although
`WasmFunction.add` assigns actual zero-based indices. It emits an explicit
`DW_OP_stack_value` only for object carriers. Consequently the original
`checksum` has a signed `int` type and a location expression, but that expression
selects the wrong local and does not explicitly describe a scalar value.
`patches/teavm-0.13.1-typed-locals.patch` uses `local.getIndex()` and emits an
explicit value expression for direct Wasm locals. It changes compiler source
metadata emission and leaves executable sections unchanged. This is a finite
scalar adaptation; it does not establish Java object graph, class inheritance,
array layout, reflection, or optimized-variable recovery.

Use another fresh build directory with
`--ordered-names --typed-locals --fixture-output wasi-debug-dwarf-name-typed-fixed`.
`build_typed_debug.py` separately compiles `TypedDebug.java` with both the
original and that private compiler cache, checks non-custom section equality,
and compares JVM/Wasmtime output for argument `17`. The loop keeps `counter`
live in a real Wasm local: a first straight-line version was optimized away
and emitted no variable DIE, so it could not establish value support. In the
retained loop fixture, at source line 10 the initialized `counter` must be
`17`; after stepping to line 11 it must be `24`. Output values are `24`, `31`,
and final `31` on both JVM and Wasmtime.
The debugger runner accepts `--arg 17 --expression counter --expected-value 17
--expected-after-step-value 24 --expected-after-step-line 11 --require-values`, making an unavailable value or
wrong numeric value fail. It verifies both `info locals` and `print` at both
stops. Source coordinates, original function names and normal termination are
also required independently.

The earlier frozen products from all three repositories passed both strict source
fixtures, including exact `info locals` and `print` values before and after
the source step. `Semantics` verifies `checksum=-2080261270` at lines 105 and
106; `TypedDebug` verifies `counter=17` at line 10 and `24` at line 11.
`evidence/2026-10-17/final-source-summary.json` pins the six reports. The first
stable `Semantics` source-over exhausted the CLI's fixed two-second operation
budget during concurrent testing; it remains a failed observation. An
exclusive rerun passed with identical inputs, command and assertions. The
operation limit was unchanged; CPU contention is a supported hypothesis,
not a proven causal diagnosis.

The current native-budget products have subsequently passed the same six cases
in a serial, exclusive CPU 21 window. `run_final_source.py` pins their exact
source IDs/binary SHAs and the original Wasm, Java source, expected stdout and
unchanged `run_source_debug.py` SHA. All ten strict checks pass for every case:
source break/step, original displayed function name, exact locals/print values
before and after the source step, guest output and normal CLI/terminal exit.
No helper, fixture, assertion or built-in two-second pause/step deadline was
changed. All six retained pidfds retire and all six `/proc` entries disappear.
Memory events are zero; post-run memory-pressure averages are 0.00, with the
existing cumulative some/full total of 2 retained in the audit.

The latest compact receipt is `evidence/native-budget-source-summary.json`;
raw per-case reports/consoles, driver, original plan and retirement audit are
under `evidence/2026-10-17/final-source-native-budget/`. The earlier failed and
passing reports remain intact. Typed claims remain limited to the two checked
primitive int variables; object graphs, arrays and optimized-away variables
are still outside the established capability.

```sh
# Only after the coordinator releases the exclusive CPU 21 window:
taskset -c 21 python3 run_final_source.py \
  --arena /tmp/high-level-20261017 \
  --project /tmp/high-level-20261017/java \
  --out /tmp/high-level-20261017/java/final-source-native-budget \
  --exclusive-cpu21-released
```

Use a fresh output directory for any subsequent execution; each failed attempt
is evidence and must not be overwritten. These are functional debugger results,
not a performance measurement.

```sh
python3 build_typed_debug.py \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --original-m2 /tmp/high-level-20261017/java/m2 \
  --patched-m2 /tmp/high-level-20261017/java/dwarf-name-typed-fixed-toolchain-v1/m2 \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --work /tmp/high-level-20261017/java/typed-debug-build \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages
```

## Guest-timed performance fixture

`PerfTimed.java` preserves the original kernel. It allocates and initializes
state before timing, puts only the kernel between two `System.nanoTime()`
calls, and folds the checksum after timing. The original `Perf.java` and
`target/perf` module remain unchanged. `build_perf_timed.py` records their
before/after hashes and builds the timed fixture in a separate work project.

TeaVM 0.13.1's unmodified `System.nanoTime` imports `teavm.nanoTime`, which a
standalone WASI host does not provide. The isolated compiler source patch
`patches/teavm-0.13.1-wasi-monotonic.patch` rewrites that method only for the
legacy WASI backend to use Preview 1 monotonic clock ID 1, preserving its
integer nanosecond timestamp. A clock error terminates with status 70. It adds
the standard `wasi_snapshot_preview1.proc_exit` import and does not use a host
stub or JavaScript timing shim. Build a separate compiler cache using
`build_dwarf_fixed.py --wasi-monotonic --fixture-output wasi-debug-dwarf-clock-fixed-v2`
and a fresh `--out`; then pass that cache to `build_perf_timed.py`.

The builder compares JVM and Wasmtime checksums at 17 and 1,000,000 iterations.
The additional reference run at 200,000,000 iterations gives checksum
`4822258612557296351` on both. `perf-timed-cases.json` supplies the same Wasm,
argument, checksum and `Kernel ns: ` clock prefix to the shared benchmark
runner. These reference executions establish correctness only; cross-runtime
timing and VTune measurements belong to the common, serialized profiling run.

## Ordinary hardware profiling with real stdin gates

`PerfProfile.java` is a distinct, compiler-produced sampling fixture. It keeps
the exact integer/array loop from `PerfTimed.java`, supports 1..2,000,000,000
iterations, warms the clock/stream objects and runs two 65,536-iteration kernel
warmups before resetting every measured input. The volatile warmup checksum is
consumed before the sampling boundary. It then uses this protocol:

1. Print and flush exactly `UWVM_PROFILE_READY\n` to **stderr**; block on
   `System.in.read()` until the controller sends the single byte `G`.
2. Read monotonic nanoseconds, run the kernel, read nanoseconds again.
3. Print and flush exactly `UWVM_PROFILE_DONE\n` to **stderr**; block until the
   controller sends the single byte `D`.
4. Fold the checksum, print the original three timed-fixture stdout lines,
   flush and exit normally. EOF or either wrong release byte fails the guest.

Both gates, warmup, initialization, checksum folding and final stdout are
outside the kernel timer. Ordinary VTune attaches while READY is blocked and
stops/finalizes while DONE is blocked; it must not use `-Rdbg`. The hardware
window includes release/clock/marker overhead, whereas `Kernel ns: ` measures
only the kernel. JVM/Wasmtime reference times are correctness evidence, not a
performance comparison. A new module and native layout are expected; no
byte/layout identity with the original timed fixture is claimed.

TeaVM 0.13.1's official
[TConsoleInputStream](https://raw.githubusercontent.com/konsoletyper/teavm/0.13.1/classlib/src/main/java/org/teavm/classlib/java/lang/TConsoleInputStream.java)
throws an EOF exception for every read. Its
[class mapping](https://raw.githubusercontent.com/konsoletyper/teavm/0.13.1/classlib/src/main/resources/META-INF/teavm.properties)
renames this class to `java.lang.ConsoleInputStream`. The additional isolated
`patches/teavm-0.13.1-wasi-stdin.patch` replaces its single-byte `read()` only
for the legacy WASI target. It calls the existing standard P1 `fd_read` import
on fd 0 with a one-byte iovec, returns -1 for EOF and exits with status 74 on
host errno/invalid read count. Bulk `read(byte[])` is not adapted or claimed.
This patch is applied after the verified monotonic-clock patch, never to the
global Maven cache or generated Wasm. No private VM import is added.

Create a new private compiler cache and independent build/oracle directory
inside the original 64 GiB cgroup (use fresh output names when rebuilding):

```sh
python3 build_dwarf_fixed.py \
  --original-m2 /tmp/high-level-20261017/java/m2 \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --out /tmp/high-level-20261017/java/dwarf-clock-stdin-toolchain-v3 \
  --wasi-monotonic --wasi-stdin \
  --fixture-output wasi-debug-dwarf-clock-stdin-fixed-v3 \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages

python3 build_perf_profile.py \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --m2 /tmp/high-level-20261017/java/dwarf-clock-stdin-toolchain-v3/m2 \
  --toolchain-receipt /tmp/high-level-20261017/java/dwarf-clock-stdin-toolchain-v3/receipt.json \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --work /tmp/high-level-20261017/java/perf-profile-gated-build-v1 \
  --iterations 17 200000000 2000000000 --case-iterations 2000000000 \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages
```

The builder checks compiler receipt/JAR identity, an identical source kernel,
and byte-identical before/after hashes for all original `target/perf` and
`target/perf-timed` files, their sources and timed manifest. It builds only in
the new work project, proves each gate waits without pre-fed input, compares
actual JVM/Wasmtime checksums for every requested argument, and rejects EOF or
wrong input at each gate. Only after every check passes does it publish the
new `target/perf-profile-gated/java-perf-profile.wasm` and flat
`perf-profile-cases.json`, containing the actual new SHA, source/compiler
identities, oracle path and `purpose: hardware-gated` gate fields. A failed or
unrun build produces no qualifying manifest. The 2-billion default is a
calibration candidate; the root runner must qualify actual ordinary kernel
duration and target-PMU sample count before accepting a profile.

The actual SSH Linux build and functional reference runs passed on CPUs 4-5.
JVM and Wasmtime agree at 17, 200,000,000 and 2,000,000,000 iterations; the last
checksum is `2861092023649889434`. Each positive reference run proves the
process remains alive without a pre-fed release byte at both gates. All eight
negative protocol runs reject EOF/wrong bytes with nonzero exits and no stable
stdout (JVM exit 1; Wasmtime exit 134). The new artifact SHA-256 is
`390f6195be3fc3443b7daa76f9568124b1583fc381881f7ae2ce38962e688933`;
the original timed artifact remains
`52505d5526e09f85c7e4b7e9781f38aabcbe7c3c35c3ca7642a280572e6757ee`.
The full original perf/timed file manifests compare equal before and after.
These executions establish functionality and do not qualify a UWVM hardware
profile or provide cross-engine performance conclusions.

Raw compiler/build/oracle/protocol evidence is under
`evidence/2026-10-17/profile-gated/`. The two earlier compiler attempts retain
their failed patch logs/receipts; the successful v3 diff includes complete
official-source context and still uses `patch --fuzz=0`. Generated Wasm was
never rewritten. The flat `perf-profile-cases.json` was published only after
all positive/negative proofs passed.

`check_owned_process.py` also passed a real Linux lifecycle test in this cgroup
on CPUs 4-5. It records a grouped helper and a detached-session descendant,
reaps their leader first, then verifies retained pidfds deliver TERM and KILL
to both helpers. A temporary subreaper reaps both, proving no owned orphan or
zombie remains; repeated cleanup is harmless. An unrelated new session with a
non-UTF-8 process name remains alive and keeps identical identity/placement
through owner cleanup, then is separately retired using its own witness
pidfd. All fixture PIDs are gone, helper source hash is unchanged, and cgroup
memory events remain zero. Its receipt and exact test/helper hashes are in
`profile-gated/owned-lifecycle-v1/receipt.json`. This test does not remove the
helper's documented between-scan discovery limitation.

## Optional Java argv and environment producer adapter

`ArgsEnv.java` checks an empty Java main array or exactly
`["", "雪=🙂", "a=b=c", "="]`, requests GC and prints exact UTF-16 units. The
unmodified TeaVM WASI provider excludes argv[0] and allocates exactly argc
pointers; it does not read an extra NULL pointer slot. Its original
`System.getenv(String)` is a null stub, and no no-argument Map getter is supplied.
The original compiled ArgsEnv module preserves that negative environment result.

`patches/teavm-0.13.1-wasi-environ.patch` is an **optional private producer
adapter**, enabled with `build_dwarf_fixed.py --wasi-environ`. It uses standard
P1 environ size/get calls, exact-count arrays, bounded NUL termination and UTF-8
decoding with a first-equals split. It implements the two normal Java getenv
overloads through the WASI class transformer and a cached read-only map. The
existing argv provider source remains byte-identical. This cache is deliberately
separate from the verified clock/stdin caches; do not combine those optional
adapter flags. No VM production code, global Maven jar, generated Wasm, engine
language flag or original fixture is changed.

`ArgsEnvAdapted.java` invokes the same ArgsEnv probe and strictly checks map
cache identity, read-only mutation rejection, named/map equality and key presence,
null name/key/value NPE, and non-string query CCE. The v3 build pairs the adapter
with the existing ordered-name and typed-DWARF producer repairs. Reproduction in
the original SSH Linux 64 GiB container:

```sh
taskset -c 4-5 python3 build_dwarf_fixed.py \
  --original-m2 /tmp/high-level-20261017/java/m2 \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --out /tmp/high-level-20261017/java/dwarf-environ-toolchain-v3 \
  --wasi-environ --ordered-names --typed-locals \
  --fixture-output wasi-debug-dwarf-environ-fixed-v3 \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages

taskset -c 4-5 python3 build_args_env.py \
  --maven /tmp/high-level-20261017/java/apache-maven-3.9.11/bin/mvn \
  --original-m2 /tmp/high-level-20261017/java/m2 \
  --patched-m2 /tmp/high-level-20261017/java/dwarf-environ-toolchain-v3/m2 \
  --toolchain-receipt /tmp/high-level-20261017/java/dwarf-environ-toolchain-v3/receipt.json \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --work /tmp/high-level-20261017/java/args-env-build-v3 --output-tag=-v3 \
  --helpers /tmp/high-level-20261017/uwvm2-ros/test/0050.high_level_languages
```

Use fresh output names when reproducing. Actual v3 JVM/Wasmtime results pass
all four argv/environment combinations and an additional JVM empty-process-env
baseline. Every stdout line is compared exactly, including populated environment
values and the checked Map contract line. The original stub module still gives
NULL for present variables on Wasmtime while the same Java source on JVM gives
the real values; this is retained as an upstream producer negative result.

The new private compiler jar SHA-256 is
`12801a92b5d1fd66cddab8a41b68e93fbeb353cbcc79b669424e6e586c6cdb12`.
The new module is `target/args-env-adapted-v3/java-args-env.wasm`, SHA-256
`78b83d7e22d2fad87af236deab9fb82bb5c16c9e1725f0ba15369374fb1b3847`.
The original stub module is retained at `target/args-env-original-v3/`, SHA-256
`170a27b2e330643af237315178cbd31dfb728e0bdec438bb5df75190a94f86a7`.
Actual imports are only P1 `fd_write`, `clock_time_get`, `args_sizes_get`,
`args_get`, `environ_sizes_get` and `environ_get`; the original lacks environ
imports. All original five modules, timed/performance modules and their complete
before/after file hashes remain unchanged. Cgroup memory events remain zero.

Evidence is in `evidence/2026-10-17/args-env/`, including the first failed strict
map test and the passing v2/v3 tests. See `evidence/args-env-source-notes.md` for
the source boundary and wrapper rationale. The producer/JVM/Wasmtime receipts
and the subsequent three-repository UWVM receipt are kept separate.

`run_args_env.py` qualifies this same v3 module against explicitly pinned
native-budget products. It uses the normal WASI environment controls,
`--wasip1-global-noinherit-system-environment` and repeated
`--wasip1-global-add-or-replace-environment NAME VALUE`; an empty VALUE remains
a real argv element. Each product gets four adapted ordinary cases, one
original-stub negative case, and one Unicode args/environment WasmDBG protocol
case. Ordinary stdout and exit are exact; stderr is retained and may contain
only the three known `[llvm-jit-full]`, `[llvm-jit-cache]`, and `[gc-managed]`
diagnostic prefixes. WasmDBG additionally checks the complete fixture output
lines, entry/step/state/breakpoint/restart and normal shutdown. It does not
claim source-language locals from a Wasm-local command.

```sh
taskset -c 24 python3 run_args_env.py \
  --arena /tmp/high-level-20261017 \
  --project /tmp/high-level-20261017/java \
  --out /tmp/high-level-20261017/java/args-env-uwvm-native-budget-v2 \
  --cpu 24
```

Use a fresh output directory. Ordinary execution is fixed to CPU 24 with the
LLVM object cache disabled. WasmDBG uses the common runner's CPUs 16-31 for
cold compilation, then fixes target execution to CPU 24. Cold commands retain
the 1200-second budget; no strict two-second Java source-step test or hardware
measurement is part of this matrix. Receipts pin actual binary/source/helper
SHAs, retain process ownership/retirement and check cgroup memory events.

The final v2 matrix passes **18/18** against the pinned native-budget products:
12 adapted ordinary cases, three original-stub expected negatives and three
Unicode environment WasmDBG cases. Empty argv/value, non-BMP UTF-8, embedded
equals, absent keys and the Map API contract all retain their exact reference
output. All owned processes retire, all three debuggers finish normally, and
cgroup high/max/OOM counters remain zero. See the small
`evidence/args-env-uwvm-summary.json` and full
`evidence/2026-10-17/args-env/args-env-uwvm-native-budget-v2/receipt.json`.

The v1 report is preserved with its exact runner SHA: every ordinary guest
stdout/exit and all three DBG cases were already correct, but that runner's
extra empty-stderr assertion rejected normal engine diagnostics. The v2 runner
changes only that stderr policy, retains the raw diagnostics, and rechecks all
18 cases. This is functionality under shared compilation load, without a
performance conclusion or a new source-language debugging claim.

## Source maps after moving an arena

Resolve a source-map sourceRoot and source name relative to the actual loaded
map location. A restored arena can therefore require a different absolute
source breakpoint path while the original Wasm/map/source bytes stay identical.
The four-product AssemblyScript follow-up pins all three and checks the
embedded source text, exact source breakpoint/step/name, full final stdout and
normal terminal restoration. A display-only source substitution is not a
breakpoint mapping.
