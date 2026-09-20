# ROS stable LLVM audit (2026-09-16–17)

This is a bounded verification record, not a claim that every platform, input or
build combination is bug-free. Earlier audit results using development LLVM
must not be relabelled as results of this new stable dependency.

Latest module-build status (September 17): attempt 06 **compiled and linked
successfully** with checked Clang 22.1.8 and bundled LLVM 23.1.1-uwvm-ros.1.
Its complete source inventory remained unchanged. The earlier pending/failure
paragraphs below are chronological records, not the final build verdict.
The fresh module CLI and a matched Clang-22 header CLI both pass the native
execution replay recorded at the end of this document. This is not a
cross-platform pass or a blanket performance guarantee.

The subsequent cache-v5 and all-registered-target audit is recorded separately
in [ros-cache-v5-audit.md](ros-cache-v5-audit.md), including its failing target
probes and the limits of the earlier v4 named-module result.

## Dependency and build contract

ROS now builds [official LLVM 23.1.1](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.1),
the latest official non-prerelease when imported, with the downstream
[MIPS R6 repair](https://github.com/llvm/llvm-project/pull/223905).
The vendored version is `23.1.1-uwvm-ros.1`. See
[the vendor README](../../third-parties/llvm/README.md) for the release commit,
archive SHA-256, verified GPG signing fingerprint, retained-source inventory,
license/provenance files and deliberate project omissions.

`xmake/utility/bundled_llvm.lua` is the only LLVM dependency discovery path.
It verifies all 12,852 retained upstream/patched files, uses CMake/Ninja to
build static LLVM archives, and reads the dependency contract directly from
CMake's file API. No llvm-config executable is built or queried.
It never falls back to `LLVM_CONFIG`, PATH's llvm-config or a system LLVM library.
A C++ bootstrap compiler and its standard library are still host dependencies;
they are not the LLVM library linked into ROS.
The ROS workflow no longer creates/queries llvm-config. It explicitly installs
CMake and Ninja (and Python on FreeBSD), removes the unneeded Linux LLVM
development-package requirement, and runs both build-contract guard tests in
the backend job with an intentionally invalid LLVM_CONFIG. The latest workflow
passes YAML parsing and syntax checks for its 41 Bash snippets; the four
PowerShell snippets and hosted CI itself were not executed
by this audit. FreeBSD's fixed `/usr/local/llvm21` bootstrap SDK path follows
[its official port](https://github.com/freebsd/freebsd-ports/blob/main/devel/llvm21/Makefile),
not LLVM dependency discovery.

The source inventory SHA-256 is
`c21b2b34295fec5a9a220edd7aa5d07228a10e6c15f4c57bbde04857994a8124`.
The cache identity includes compiler bytes, source/patch bytes, relevant flags,
targets and cross configuration. The build recipe has an additional stamp, and
Ninja checks missing outputs on every xmake invocation. A copied CMake cache
pointing to another source checkout and unsafe compiler FP flags are rejected.
These checks prevent accidental substitution/staleness; they are not a sandbox
against an attacker controlling the build tools, manifest or filesystem.

Normal usage:

```sh
xmake f --execution-int=uwvm-int --execution-jit=llvm --llvm-build-jobs=2
xmake -j1 uwvm
xmake lua test/0018.build/check_bundled_llvm_manifest.lua
xmake lua test/0018.build/check_bundled_llvm_contract.lua
```

The first configure builds LLVM. `--llvm-build-targets=all` is useful for backend
audits, not required for a native application. `--execution-jit=none` needs no
LLVM dependency. Cross LLVM libraries require an explicit matching CMake
toolchain; no target runner is required for dependency discovery, and silently
linking the host LLVM is forbidden.

## New stable-version compatibility changes

LLVM 23.1's `OptimizationLevel` is an enum, unlike early LLVM 23 development
headers. Both ROS and ordinary uwvm2 now select speed optimizations by comparing
the actual O2/O3 public values, preserving the previous O0/O1 policy without
relying on the removed `getSpeedupLevel()` or enum encodings.

Test-only API repairs are also paired: `lookupTarget` uses the long-standing
three-argument overload. No-inline/native-unwind tests retain all five
speed/size policies; where Os/Oz no longer exist, they use O2 with optsize/minsize
attributes, as prescribed by the stable LLVM PassBuilder diagnostic. Removing
the size cases would hide coverage rather than repair the API compatibility.

Stable 23.1.1's X86 `lowerFPToIntToFP` tracks both conversions' signedness
independently. The unconstrained conversion regression therefore passes, and
ROS can use direct integer-to-float emission again. Ordinary uwvm2 still accepts
other LLVM installations and retains the development-LLVM workaround. This is
not permission to remove NaN-bit, FP-environment or trapping-conversion guards.

## Verified snapshot and resource limits

The fresh ROS CLI source snapshot manifest is
`sha256:17f74c31eee1c5c15915d7987fd5b823549b3f14aaa531e23a132b5d3aea84c4`.
The first stable-dependency CLI SHA-256 was
`66a09701c5b195a4bb42fb669c79cbd4a9f5a43b0bf99ff08ddaaadee3cd9b57`.
After replacing llvm-config with the direct CMake contract, the rebuilt CLI is
`05c1656b807fb67c9a64ac7ce84d5e7cacb3338f0393e5f4e49add99214fd7a2`.
All source files in that snapshot were verified before compiling. Subsequent
test-driver and build-check improvements did not change production C++ sources;
their logs record their separate commands/source identities.

Configuration: Linux x86_64, release O3, native ISA, header mode, interpreter
plus full LLVM, soft combined ops, heavy delayed locals, loop unwinding,
libc++/compiler-rt/libunwind, signed OpenSSL cache. LLVM was built Release with
all backends, eight compile jobs and one link job; ROS C++ builds were serial.
The bootstrap compiler was the installed LLVM 23 development SDK, while **all
LLVM dependency headers/libraries/tools under test were stable 23.1.1**.
`ldd` showed no dynamic libLLVM dependency. Deliberately invalid LLVM_CONFIG
did not affect configuration, build or incremental rebuilding.
The old bundled llvm-config executable was then moved out of the build tree.
A successful incremental build, with execve tracing, neither invoked nor
recreated it. The metadata-only CMake target also remained unbuilt. The trace
used `ulimit -c 0` without the usual non-dumpable preload (which prevents ptrace);
all memory/CPU limits and the regular VM-test no-core guard remained in force.
Core 2, memory, integration, unwind/cache, conversion and exhaustion CLI tests
below were all repeated successfully on this final no-config executable.

All substantive Linux jobs used the same aggregate cgroup: 32 GiB MemoryMax,
28 GiB MemoryHigh, no swap, CPUs 16–31 (16 E-cores). `--llvm-build-jobs` alone
is not a memory bound and must not be advertised as one.

## Completed tests

| Check | Result |
| --- | --- |
| Source inventory/build guard tests | Positive controls and 7 expected negative cases pass |
| CMake dependency contract | Ordered/repeated static archives and paths with spaces pass; 12 negative cases reject invalid metadata/configurations, versions, library substitution, escape and missing archives |
| Official archive comparison | 12,852 retained files; exactly the documented 2 backend edits and 3 added MIPS tests |
| Header version pin | Old 23.0 development headers rejected; bundled 23.1.1 accepted |
| OptimizationLevel speed-policy API | Old class and stable enum both pass |
| MIPS LLVM regressions | All 10 RUN lines in the 3 retained tests pass |
| MIPS32r6 O32 execution | 12 endian/optimization profiles; 2,409,600 vector selections and 4,819,200 full-width f64 spill roundtrips pass |
| MIPS N32/N64 | 4 object/codegen checks pass; not runtime execution |
| Raw x86 mixed-signedness conversion | 14 executions, 1,876 exact-bit checks; SSE2, AVX2, AVX512 with/without VL, x86_64 no-SSE, i686 SSE2/x87 at O0/O3 |
| Core 2 execution through ROS interpreter/full LLVM | 2,296 runs, 83,152 assertions, zero failures |
| Actual CLI SIMD memory boundaries/grow | 652 checks pass |
| Full-feature/import-alias/DataCount integration | 65 checks pass |
| Recursive unreachable/OOB/float-to-int stacks and signed cache | 108 CLI runs, including 72 authenticated replays, pass for instruction and native unwind tracking |
| CLI mixed-signedness conversion | 13 runs, 104 assertions, 6 authenticated replays, zero failures |
| Native stack exhaustion | 150 cases pass, with tracking none/instruction/unwind and debug/O1/O3 policies |
| Production SIMD cross-codegen | 15 target profiles each pass all 236 opcode comparisons and 134 guarded-store boundary/no-partial-write cases |
| Named section-memory-manager partition | 11 real module/partition builds pass on Linux with the bundled headers |
| Interpreter-only configure | Passes with the entire LLVM vendor directory absent and invalid LLVM_CONFIG |
| Native LLVM tests | 35 executed test programs pass after the stable OptimizationLevel test-API repair; the 36th batch backend probe is tested by the parity driver below |
| Standard/translator control parity | 14,438 comparisons pass using the new ROS CLI/backend probe and the unchanged v6 ordinary/standard-parser baseline probes |
| Actual interpreter assembly | All 18 inspected scalar/vector floating-point add specializations use native FP instructions, no calls or native-stack accesses, and tail dispatch |

The native-unwind provider test includes 20 recursive chains with five exact JIT
frames each, 10 object compilations and 30 cache replays across five speed/size
policies. The repaired no-inline and unwind-provider tests also compile/run
against the ordinary version's older LLVM development API.

## Performance and generated code

The alternating 15-round microbenchmark uses 10,000,000 iterations of scalar,
f64 and v128 kernels, measures guest execution (not compilation/startup), and
requires a warm object-cache hit. Relative to the previous ROS v6/development-
LLVM executable, median interpreter time was 187.14 ms versus 172.65 ms and JIT
time was 21.761 ms versus 21.787 ms (ratio 1.00124). These are limited measurements,
not a general speedup claim or a platform-wide regression bound.

Disassembly of authenticated cache objects confirms that all three JIT kernels
and the entire 1,574-byte executable text are byte-identical between those two
snapshots; object metadata differs. The actual interpreter binary inspection
covers eight f32-add, eight f64-add, one f32x4-add and one f64x2-add specialization.
It does not assert that every opcode on every ISA is free of helper calls.

The SIMD profiles are x86_64, i686 x87/SSE2, AArch64 with/without SIMD, ARM
NEON, PPC32, PPC64 BE/LE, RISC-V scalar/vector, SystemZ, LoongArch LSX/LASX and
SPARC64. These execute generated objects with target FP lowering/legalization
and QEMU where needed, **not** a full ROS CLI/JIT/OS build on every target.
SIMD arithmetic NaNs are compared under the permitted Wasm result rules;
bit-preserving operations remain exact, including signaling NaNs and signed zero.

The Core 2 execution driver explicitly excludes 4,673 external/stateful script
actions it cannot model; it is not whole-spec certification. MIPS O32 includes an
unpatched-backend negative control. Initial test-launcher attempts with an
invalid unwind mode, missing host libc++ search paths, or inherited
`-fno-exceptions` are retained as failed setup evidence, not passing coverage.

## Comment review and follow-up configuration/runtime checks

The [maintenance rationale](ros-llvm-maintenance.md) explains the exact release
pin, no-system-fallback rule, ordered static dependency contract, cache/lock
lifetimes, host FP requirements, OptimizationLevel size-policy compatibility,
direct conversion lowering and GCC/i386 O0 signaling-NaN ABI boundary. Comments
beside the implementation record why each constraint must survive maintenance.
The native tail-call comment was corrected in both products: physical frames
are required by this runtime's diagnostic policy, not mandated by Wasm's native
implementation model. Four ROS C++/test files match the running build snapshot
byte-for-byte after removing standalone `//` lines; macro continuations were
checked. These edits do not change C++ statements, macros, signatures or hot-path
branches; this equivalence check is not a new generated-code performance test.

Review found and fixed three build-configuration acceptance gaps:

1. A toolchain could replace the requested Release build with Debug. Both the
   actual deferred CMake hook and the file-API reader now require Release; one
   configuration alone is not enough to establish the intended ABI policy.
2. A genuine AArch64 GCC configuration without LLVM_HOST_TRIPLE succeeded but
   selected X86, because upstream config.guess saw the x86_64 build machine.
   Cross toolchains must now supply that triple before LLVM inserts a default.
   xmake clears old inferred host/default triple cache entries before reading
   the toolchain, so stale successful configurations cannot bypass this check.
3. Upstream LLVM only warns if its native backend is omitted. ROS requires that
   backend for JIT and now rejects such a configuration before building libraries.

`test/0018.build/check_bundled_llvm_cmake.py` configures actual vendored LLVM;
it does not build the libraries or execute a target tool. Linux x86_64 and
macOS arm64 each pass Release, forced-Debug rejection, missing-native-backend
rejection and the explicit cross-request/missing-triple guard even when CMake
does not mark the build as cross-compiling. The Linux run also passes real
AArch64 missing-triple rejection and
explicit-triple/AArch64-backend selection. A separate reconfigure of the actual
pre-fix wrong-triple cache is rejected with the new reset/guard. The current
native archive directory passes the Lua contract reader with 62 ordered static
archive entries. The configure-only driver is included in the ROS backend CI
job; CI itself has not been run here.

The fresh `test/0017.runtime/check_runtime_guard_matrix.py` run passes all 40
compiler/profile cases: GCC 15.2 and Clang 23 bootstrap at O0/O3, plus Clang O3
UBSan. This includes 35 executions of runtime test programs and five compilations
of the compile-time cache-source policy (whose main is empty). Coverage includes
stack exhaustion, warmed/uncached alternate stacks, concurrent first entry,
thread teardown/re-entry, host alternate-stack restoration, live stack limits,
signal registration/forwarding, frame-size overflow, generation/table reset and
runtime-state reuse rejection. These helper tests do not replace full VM or
cross-OS coverage. Final logs/results are in `ros-comment-runtime-guards-final`;
the runner kills its own child process group on timeout, avoiding orphan compiler
or test processes in the shared memory budget.

The configuration checks above are recorded under
`/var/tmp/uwvm-comprehensive.kH4XCt/ros-comment-cmake-cross-final4`,
`ros-comment-cross-contract-before` and `ros-comment-cross-cache-rejected` on
Linux, and `/tmp/uwvm-ros-llvm23.GQTbOH/ros-comment-cmake-darwin-final4` on the Mac.
They do not imply a full cross-built AArch64 LLVM/ROS executable has been tested.
An intermediate repeat (`cross-final2`) timed out during its Release positive
control while the x87 compile and module build were under memory pressure;
it is retained as failed setup, not a passing check. About 3.3 GiB of inactive
SDKs were then copied with checksum comparison from the campaign's tmpfs to
ordinary disk, leaving the original path as a symlink and removing only the
verified duplicate. The aggregate 32-GiB limit was never increased.

A fresh i686 FP/ABI run passes **168/168** cases with GCC 15 and the Clang 23
bootstrap compiler: x87, SSE2 and SSE4.1 profiles at O0/O3, covering scalar,
boundary, SIMD and tail-call fixtures. Results are in
`/var/tmp/uwvm-comprehensive.kH4XCt/ros-comment-fp-recheck/results.json`.
These execute actual interpreter helpers under QEMU, not a full cross-built CLI
or the bundled LLVM JIT. During the last large concurrent compilations, the
soft MemoryHigh threshold was temporarily 30 GiB and then restored to 28 GiB;
the aggregate hard MemoryMax remained 32 GiB, swap remained disabled and all
campaign jobs stayed on CPUs 16–31.

The full module build exposed a separate diagnostic-template expansion problem.
The full-policy callback's original BMI was 3,767,587,824 bytes. Splitting its
17 conditional color operations into bounded print packs, under a single outer
stream lock, reduces the isolated real module BMI to 21,259,432 bytes. That
compile took 19.59 seconds with 4,320,588 KiB maximum RSS. The local RAII guard
avoids depending on an unexported fast_io module helper; native Windows color
manipulators remain intact. This cold CLI fix is applied to both products and
does not alter policy selection or Wasm/JIT code generation.

The real callback body passes 320 isolated fixture cases (two products × two
declaration forms × GCC/Clang × O0/O3 × 20 cases), including byte-exact color and
no-color output and 8,192 concurrent diagnostic records. Results are in
`/tmp/uwvm-comprehensive.eUFnxH/ros-comment-callback-output-final2`; the successful
real module compile is `ros-comment-callback-bmi-final` under the disk directory.
The fixture stubs parameter registration and the usage-printer result; the
header-form test selects the real constexpr qualifier but stubs umbrella
includes. Neither fixture is a complete CLI or legacy Windows console test.
Earlier attempts exposing the unexported lock helper and using the ROS wording
for the ordinary-product oracle are retained as failures, not counted as passes.
A native macOS arm64 repeat with Apple Clang 21 passes another 160 cases (both
products, both declaration forms, O0/O3), including 4,096 concurrent diagnostic
records. It uses the real Darwin native sink/mutex but is still an isolated
callback fixture, not a full Darwin module/CLI build. Its results are in
`/tmp/uwvm-ros-llvm23.GQTbOH/ros-comment-callback-darwin/results.json`.

Resuming the complete build found a fourth build-cache issue: using a symlink
alias for the same LLVM cache changed CMake's absolute include paths and caused
Ninja to recompile unchanged LLVM sources. The cache now passes through CMake's
`file(REAL_PATH)` before locking/configuration. The new
`test/0018.build/check_bundled_llvm_paths.lua` passes on Linux and macOS, including
absolute/relative symlinks, spaces, unchanged process CWD and missing-directory
rejection. It uses the real CMake resolver but does not establish Windows
junction behavior. The original obsolete 3.5-GiB diagnostic BMI was removed
after recording its hash/size; it is a rebuildable intermediate, not source.
The resumed build uses a new source manifest/identity rather than labeling the
revised source as the earlier successful header-mode snapshot.
After rebuilding the libraries at the canonical path, explicitly relinked
`llc`/`FileCheck` pass all ten retained MIPS RUN lines again; llvm-config remains
absent. The fresh tool hashes and logs are `ros-comment-canonical-mips.log` and
`ros-comment-canonical-mips/results.json` in the campaign root. This is a backend
regression repeat, not a new MIPS full-CLI execution result.

A fresh comparison using the actual ROS production SIMD BMI passes all eight
rounding wrappers: header/module instruction and operand sequences are identical,
with one `vroundps`/`vroundpd`, a result store and return, no helper call or stack
access. The BMI hash is
`9224f03b5b661b02dfde539416d7d80247e805049c772a4d3efc407de987d00b`;
results are in `ros-comment-module-rounding-final/`. An earlier attempt failed
before code generation because the fixture included `<array>` after the import,
triggering a Clang 23/libc++ `__promote_t` redeclaration. Both products' fixtures
now include their standard headers before the import, matching production
global-fragment ordering. This remains a shared-evaluator assembly check, not
a linked module CLI or every SIMD instruction.
The driver's new opt-in `--execute-fixture` repeat additionally links each
object separately and passes 792 independent bit-oracle assertions in each
form (1,584 total). This executes the real shared evaluator instantiated from
the production BMI; it is still not full VM execution. Results/commands and
both disassemblies are in `ros-comment-module-rounding-executed/`.

## Build-contract follow-up

A further configuration review reproduced an unsafe-flag detection gap:
`add_compile_options(-ffast-math)` need not update `CMAKE_CXX_FLAGS`. The
production reader now checks the resolved consumer compile fragments, before
building `llvm-libraries`. The synthetic contract suite now passes 13 negative
cases (the new one failed before the fix). A fresh real CMake run on Darwin
passes all five cases, including a project hook adding that directory option;
the actual CMake reply is accepted/rejected by the production Lua reader, not
merely inspected by a duplicated Python oracle. Results are in
`/tmp/uwvm-ros-llvm23.GQTbOH/ros-comment-cmake-resolved-fp/`. The active Linux
module build retains its earlier strict configuration and frozen input snapshot;
this additional configure-only guard is not claimed as part of that build.
Arbitrary executable toolchain scripts remain trusted inputs, not a sandbox.
The final follow-up expands this to six real CMake cases on both Darwin and
Linux: an initially precise included hook is changed to add fast-math, then
`build.ninja` is refreshed without compiling an LLVM archive and the new reply
must be rejected. The recipe now performs that build-system-only refresh before
its flag checks and rereads the contract after building libraries as well.
Final results: `ros-comment-cmake-regen-final/` on the Mac and
`/var/tmp/uwvm-comprehensive.kH4XCt/ros-comment-cmake-resolved-fp/` on Linux.
Linux uses a separate `ros-comment-guard-snapshot` with a read-only source-tree
symlink, so the active VM build's inputs are unchanged. The new production Lua
reader also accepts the actual existing bundled-LLVM contract. The first
isolated setup omitted utility/common.lua and stopped before any CMake tests;
that setup failure is not counted as a product/configuration pass.
The flag guard also rejects clang-cl forwarding spellings (`/clang:`, `-clang:`)
and joined `-Xclang=` options. The manifest/flag suite now passes ten negative
cases on both hosts; an actual clang-cl preprocessing probe confirms that
`/clang:-ffast-math` defines `__FAST_MATH__`, so the prefix cannot be treated as
an inert string. The resolved-contract suite remains 13 negative cases.

The same review found a Windows CRT recipe defect: this pinned LLVM tree no
longer reads `LLVM_USE_CRT_RELEASE`. ROS now maps MT/MD/MTd/MDd to CMake's
`CMAKE_MSVC_RUNTIME_LIBRARY` and checks each requested LLVM component's actual
target property. A six-case CMake/Clang COFF regression passes: all four modes
match `_DEBUG`/`_DLL` assertions and archive defaultlib directives; the legacy
MDd option demonstrably emits `msvcrt.lib` rather than `msvcrtd.lib`; a target
property overridden from MDd to MD is rejected by the real ROS CMake hook.
Results are in `/tmp/uwvm-ros-llvm23.GQTbOH/ros-comment-crt-coff-final/`.
The tiny fixture substitutes the LLVM component graph and compiles header-free
COFF; it does not link a Windows CRT, build LLVM for Windows or run Windows.
Its bootstrap is Homebrew Clang 20.1.8, not the pinned LLVM dependency. An initial
attempt failed in compiler detection because an inherited Darwin library path
selected a different libclang-cpp; the successful repeat clears that environment
only for the test process. Neither the user's environment nor SDK was modified.
A second six-case matrix using GNU-style clang++ also passes in
`ros-comment-crt-gnu-final/` (12 passing cases across the two frontends). Its
initial attempt caught an overly narrow guard in the candidate fix: CMake's
MSVC boolean can be false for a GNU frontend targeting the MSVC ABI. The hook
now also recognizes `CMAKE_CXX_SIMULATE_ID=MSVC`, while retaining the exact
target-property check. The failed candidate logs remain in
`ros-comment-crt-gnu-before/`; they are not counted as successful configurations.
The final hook also passes a repeated clang-cl matrix in
`ros-comment-crt-cl-final2/`, after adding GNU-frontend ABI recognition. The Mac
CI job now includes both COFF matrices; workflow YAML and 41 POSIX shell snippets
parse successfully, but hosted CI and its PowerShell steps have not run here.

## Named-module completion and remaining boundaries

The original whole named-module run was stopped by its disk guard at about 42%
when free space fell below 2 GiB. It did not complete; do not infer success from
the successful header-mode build and isolated partition tests. Windows,
Darwin, every ISA/sub-ISA/ABI, all interpreter build-option combinations and
complete cross-built LLVM libraries are not established by this Linux audit.
Stable release provenance and targeted regressions do not constitute a proof
that LLVM or ROS has no remaining bugs.
The full ROS module build uses `/var/tmp/uwvm-comprehensive.kH4XCt/ros-vendor23-modules`
for reclaimable on-disk BMIs, shares the same verified bundled LLVM archives,
and runs serially inside the same 32-GiB aggregate cgroup. A separate guard
stops only its observed build scope if the shared disk falls below 2 GiB free;
this is not permission to delete another agent's data. Its log is
`/tmp/uwvm-comprehensive.eUFnxH/ros-no-config-modules.log`.
The revised run is `ros-comment-modules-final2.log`; its inputs are recorded in
`ros-comment-modules-final2.manifest.json`. An earlier partial `xmake f`
invocation reset omitted options and was stopped before library compilation;
the corrected driver repeats the full configuration. The symlink-triggered
duplicate LLVM build was also stopped before retrying with physical paths.
The revised run then failed at about 43%: `loader.cppm` did not directly import
`uwvm2.utils.container`, although its WASI initialization/group validation names
`u8string_view`. Both products now declare that dependency in the header and
module. The ordinary product also lacked the WASI feature-header setup in its
global module fragment; imports cannot propagate those macros and the missing
setup can silently omit initialization/validation. The new real-preprocessing
regression passes hosted, explicitly disabled and missing-provider negative
cases on Apple Clang 21 for both trees, and GCC 15/Clang 23 bootstrap for ROS on
Linux. One initial Linux test invocation lacked the SDK's runtime library path;
it failed before preprocessing and is not counted as coverage.
An isolated replay of the actual failed loader compilation, with real dependency
BMIs and production flags, now passes; removing only the container import
reproduces the original missing-`u8string_view` diagnostic. Both positive and
negative-control commands/results are in `ros-comment-loader-bmi/`. These
isolated outputs never replace the production build's BMIs.

The incremental import-diagnostic continuation is in
`ros-comment-module-imports-01/`. Its sidecar records every input delta against
the base manifest. The embedded global build ID is deliberately frozen while
locating further import failures, to avoid rebuilding every BMI for every
iteration. Such artifacts are diagnostic only: do not distribute them or their
object caches as a release with the base ID. A release build needs an updated
source identity. Crossing the first failed partition is not yet a whole-build
or runtime pass.

That first incremental attempt subsequently failed around 47% in the full
interpreter's SIMD translator: `v128_unop` belongs to
`uwvm2.runtime.compiler.shared.wasm1p1_simd`, and importing optable exposes its
namespace alias but not that declaration. The ordinary product already has the
direct dependency; ROS now matches it in both module and header forms. An
isolated replay using the actual failing command and dependency BMIs passes;
removing only that import reproduces the original error. Results are in
`ros-comment-interpreter-translate-bmi/`. The module checker and all 39 checker
semantics tests also pass after this repair. These are not full-build passes.
A separate ordinary-header semantic compile, retaining the production feature
and compiler flags but removing module inputs, also passes; its command/log are
in `ros-comment-interpreter-translate-header/`. It does not instantiate and
execute every translator configuration or replace the pending full-CLI replay.
The backend CI now runs those inexpensive checks even while the whole-module
job is disabled; the updated workflow still passes YAML/41 shell syntax checks.
The next continuation is `ros-comment-module-imports-02/`, with sidecar SHA-256
`e5d2b90fda1bfbd0b79fc73e0050a2f7f765c469c872bd8f9e1134b8a7e4fe4c`.
Its diagnostic-only identity limitations are the same as the first continuation.

The subsequent local shared-source guard passes all 49 deliberately paired
files and its nine checker regressions. The report is
`/tmp/uwvm-ros-llvm23.GQTbOH/ros-comment-shared-parity-final.json`. This permits
the documented ROS control-flow/comment differences and checks that removed
lazy/tiered paths stay absent; it is not whole-tree identity or Wasm conformance.

The new reusable `check_existing_int_add_codegen.py` passes on the previously
verified ROS header-mode binary (SHA-256 `05c1656b807fb67c9a64ac7ce84d5e7cacb3338f0393e5f4e49add99214fd7a2`):
eight f32, eight f64, one f32x4 and one f64x2 add specialization retain native
adds and jumps, with no calls or RBP/RSP references. Reports and assembly are
in `ros-comment-header-int-add-assembly-final/`; this is 18 actual linked
functions, not all opfuncs. Selection regressions cover header/module spellings
and non-add rejection. A real tiny Clang module confirms that module ownership
annotates both the function and enum names; the earlier candidate selector
only handled the latter and has been corrected before the pending module replay.
Both products have the same checker. It rejects an empty selection and records
the binary/checker hashes. The module build's final binary is not yet covered.
The stricter follow-up also rejects implicit stack instructions
(push/pop/ret/enter/leave) and requires an indirect jump, rather than treating
any local jump as dispatch. All 18 functions still pass in
`ros-comment-header-int-add-assembly-strict/`; four checker unit tests cover
symbol spelling and these negative assembly cases. The backend CI runs those
small unit tests, not the architecture-specific binary inspection itself.

To keep disk headroom, the 36 old, completed unit-test executables (2,471,857,048
bytes) were archived, compared against the originals and copied to the Mac with
matching SHA-256 before their remote copies were removed. Sources and results
remain. Recoverable archive in `/tmp/uwvm-ros-llvm23.GQTbOH/`:
`ros-vendor23-unit-binaries-20260916.tar.zst`, SHA-256
`1c873770fd99bd3b70d107a354f1acac942c7d418536993576b3f16316a62859`.
Copy it back and extract under `/var/tmp/uwvm-comprehensive.kH4XCt/` to restore
the original binary paths; existing links to those binaries are dangling until
restored. Individual executable hashes are recorded in
`ros-vendor23-archived-unit-binaries.json` alongside the archive on the Mac and
in the Linux disk campaign directory. No other agent's files were removed.

Working evidence is under `/tmp/uwvm-comprehensive.eUFnxH/ros-vendor23*` on the
Linux test machine; the fresh source is `ros-vendor23/`. Reports, generated
IR/assembly and failure logs distinguish exact snapshots and actual execution
from compile-only checks. The no-config rerun and performance/assembly evidence
are in the adjacent `ros-no-config*` directories and logs.
The completed reports, compiler logs and inspected assembly (excluding cache
signing material and build binaries) are also preserved as
`ros-llvm23-no-config-evidence-20260916.tar.gz`, SHA-256
`3e5ed2453769809ec878d7f835f74e258d11a5516998693d3003cb48b8bc9961`,
in `/var/tmp/uwvm-comprehensive.kH4XCt/` on Linux and
`/tmp/uwvm-ros-llvm23.GQTbOH/` on the Mac. This archive predates the result of the
whole named-module run and must not be used as evidence that it passed.

The completed follow-up helper/configuration/FP/callback/backend evidence is
separately archived as `ros-llvm23-comment-review-evidence-20260916.tar.gz`
(711 files, SHA-256
`9c01b330dcf18892c30815500776e58df37e465d837c008e5d1161eeecb1035e`),
in both locations above. Its internal manifest identifies every file; earlier
failed attempts are retained and distinguished from final results. It excludes
binaries, signing material and the revised whole-module build (which later
failed at the missing loader import described above).

Completed loader positive/negative controls, SIMD BMI assembly/execution,
real CMake regeneration/FP guards and CRT COFF checks are additionally archived
as `ros-llvm23-module-followup-evidence-20260916.tar.gz` (2,534 files,
SHA-256 `e3ca389f5fb3434987258c73b3c0d725a0af20c3229f1e8f92f464ad2083fa3e`),
in both evidence locations. It includes the failed earlier module-build log,
not the ongoing incremental build, and predates the three final forwarding-flag
negative tests. It contains no VM binaries, BMIs or cache signing material.

The subsequent failed incremental attempt 01, its SIMD-import repair controls,
ordinary-header semantic check and 18-function header-binary assembly checks
are archived as `ros-llvm23-import-repair-evidence-20260916.tar.gz` (60 files,
SHA-256 `9e6302ccb984bb7e99352d9f532209972216db2994bcb2a1b26c727c0365676b`)
in the same Linux/Mac evidence directories. Every member digest was verified,
and the copied archive digest matches. Attempt 02 contributes only its input
manifest, not an unfinished log or a claimed pass; no binary/BMI/key is included.

## Runtime API / final-entry module follow-up

Incremental attempt 02 subsequently failed in `run.cppm`: its global-fragment
API include could not see the module-owned `u8string_view`. Replacing it with
`import uwvm2.runtime` exposed a second, independent defect in the API: an
exported preload descriptor forward declaration attached a foreign type to
`uwvm2.runtime`. The API now imports the descriptor's actual owning module and
keeps that forward declaration only in textual builds. Both products are fixed.

To avoid restarting all interfaces for every late failure, subsequent semantic
preflights use xmake's actual dry-run commands and existing successful BMIs.
Changed/missing BMIs go to isolated report directories; production outputs are
never replaced and missing object files are not faked. Attempt 03 used an invalid
dry-run option and is a setup failure. Preflights 04/05/06 respectively found the
descriptor ownership conflict, the unexported network-service alias and missing
direct runtime dependencies/coroutine header. These are retained failures.

Preflight 07 passes all three implementation TUs and the nine changed/missing
interfaces they require. Its run.cppm negative control restores the original
global API include and reproduces the original visibility error. Results are in
`ros-comment-module-preflight-07/`; 1,268 interface commands in the dry-run are
not 1,268 newly compiled interfaces. This is semantic compilation, not a linked
CLI pass. The API-specific replay in `ros-comment-runtime-api-ownership/` also
passes both actual production positive commands, negative-BMI setup, the expected
consumer ownership rejection and two textual-header include orders.

The remaining fixes use the module-exported Windows WSA RAII type (unchanged
lifetime) and directly include/import coroutine, memory printer, global reference
and hash providers in the runtime implementation. Ordinary uwvm2 received the
same fixes and regression drivers. The platform selector passes six preprocessing
cases each for Apple Clang 21 in both products, and Clang 23/GCC 15 on Linux ROS;
these are not Windows execution results. Both dependency scanners and all 44
checker regressions pass. Workflow YAML and 41 POSIX shell snippets pass again;
hosted CI itself has not run here.

The follow-up header semantic matrix in `ros-comment-runtime-header-profiles-02/`
passes interpreter-only, LLVM-only, both and backend-free stub implementations.
The two no-LLVM cases remove every vendored LLVM include directory. This reuses
production compiler flags with the actual backend enable/disable macros; it does
not link or execute those four configurations. The initial helper omitted the
required disable macro and was correctly rejected as an invalid configuration;
that failed setup is retained in `ros-comment-runtime-header-profiles/`.
The paired-source guard now covers 51 files, including the API module and CRT
entry, and passes its eleven checker regressions. This is deliberately not a
requirement that ROS restore ordinary uwvm2's removed execution modes.

The additional interpreter option matrix passes all 48 template-instantiation
profiles in `ros-comment-int-compile-profiles-02/`: combine none/soft/heavy/extra,
delay-local none/soft/heavy, instruction reorder off/on and loop unwind off/on.
It uses the existing strict f64 subtraction fixture, instantiating both tail and
non-tail translators under the native x86_64 production compiler baseline.
The first driver attempt omitted xmake's existing test-only UWVM_TEST and
undefined-inline policy and failed on CLI callback declarations; the corrected
driver matches that policy without changing production warnings. This is not
48 linked CLIs or execution/assembly coverage of every instruction/configuration.

Four representative profiles subsequently link and execute successfully:
none-heavy-reorder0-unwind1, soft-heavy-reorder1-unwind1,
heavy-heavy-reorder1-unwind1 and extra-heavy-reorder1-unwind1. The existing
fixture compares four finite f64 subtraction results through both tail and
non-tail runners in each binary (32 value comparisons total), and checks the
selected extra-heavy fusion's bytecode. It retains runtime scheduling defaults;
enabling the compile-time reorder switch is not a test of every runtime reorder
policy. Results, exact commands and binary digests are in
`ros-comment-int-profile-exec-01/` and `ros-comment-int-profile-exec-02/`.
This does not establish NaN semantics, bit-exact signed zero or throughput.

Full incremental build attempt 03 has input-manifest SHA-256
`7ee099434c233cd531dfa7be1e18a278e5ade31ad214c517f0e1c6e41783ed29`.
Its interfaces and implementation TUs compiled, but final linking failed with
duplicate module initializer symbols: CLI and runtime each owned the same
interfaces, and the object-library dependency linked both copies. This is a
retained failure, not a passing whole build. Production module objects now
belong to the runtime target; public interfaces remain available in Release.
Runtime-backed test targets have received the same registration correction in
both products. Different compatible consumer BMIs do not justify duplicate
initializer objects, and no duplicate-symbol suppression was added.

Incremental attempt 04 has input-manifest SHA-256
`a6b809710f80cef9f8560d0d13a3b056504bf4d5975db7b2b89d0676cde5a108`.
It retains the diagnostic-only base identity limitation described above. It was
deliberately stopped (return code -15) after detecting a separate bootstrap
initializer defect; it has no passing linked binary or execution replay.
Its frozen xmake input includes the production graph correction; subsequent
local changes additionally correct test registrations without changing CLI TUs.

The focused ownership regression passes on Linux Clang 23 and on macOS Clang
20 for both source trees: single-owner build/execute, duplicate-owner link
rejection, private-interface dependency rejection, plus positive/negative source
registration checks. Results are in `ros-comment-module-ownership-03/` on Linux
and `ros-comment-module-ownership-mac-03/` /
`main-comment-module-ownership-mac-03/` in the Mac evidence directory.
Earlier fixture attempts are retained. The exported-variable fixture isolates
object ownership but misses a separate internal-variable initialization defect;
the original internal-variable failure must not be dismissed as a fixture flaw.
The first Apple Clang attempt rejected named
module syntax, and the second driver recognized compiler but not xmake's precise
missing-dependency diagnostic. Those are not product passes or Windows coverage.

Further IR/assembly inspection and the upstream
[initializer fix #218304](https://github.com/llvm/llvm-project/pull/218304)
identified the independent Clang snapshot bug. A one-phase candidate also failed
locally and was reverted; no such workaround is shipped. Both products now
reject a Clang module configuration when the actual full-BMI-to-IR probe loses
the non-inline internal initializer call. No target binary is executed by this
probe. Direct checks accept system Clang 22.1.8 on Linux and Clang 20.1.8 on macOS,
and correctly reject the campaign's old Clang 23 snapshot. The next complete
module build must use a passing bootstrap; its linked LLVM remains bundled
23.1.1, not the bootstrap compiler's major version.

The checked Linux Clang 22.1.8 subsequently passes the actual combined-backend
runtime header TU's semantic compilation with the same libc++ SDK and bundled
LLVM headers (`ros-comment-safe-bootstrap-preflight/`). The three focused
object-ownership controls also pass with that compiler
(`ros-comment-module-ownership-clang22-02/`); the preceding fixture setup lacked
Clang's bin directory in PATH and is retained as a setup failure. These checks
are not a full module link or VM execution result.

Fresh module build attempt 05 uses source-manifest SHA-256
`f464fc3f17351f0628f4c7567b0971122eaa0c5556dfd4316a94bf1f45cced85`
as its actual embedded identity, not attempt 04's frozen diagnostic identity.
Its real build driver first accepts Clang 22 and rejects the known-broken Clang
23 snapshot, then rebuilds bundled LLVM under the new compiler/ABI cache key.
LLVM used four compile jobs and the VM one, within the same aggregate 32-GiB,
zero-swap, CPUs-16–31 limit. The LLVM library build and production initializer
probe passed, but the first VM dependency scan failed: xmake deduplicated the
standalone `-isystem` tokens in the launcher's raw `--cxxflags`, leaving a libc++
directory as an extra source input. Attempt 05 is therefore not a VM build pass.
The actual failed scan is reproduced by a negative control; replacing that
argument with the joined `-isystem/path` spelling passes the same scan
(`ros-comment-scan-include-flags/`).

Attempt 06 retains the same frozen source-manifest identity and corrects only
the launcher flag spelling. Its LLVM cache key changes normally with those
flags; no old archive identity is forced. It uses eight LLVM compile jobs and
one VM job under the same hard memory/CPU limits. The complete module build
subsequently passed (return code 0, 6,495.68 seconds for the VM build step;
690.05 seconds for configuration including the LLVM build). The input inventory
remained unchanged, and the executable SHA-256 is
`d0ecb2195031e69f973ceec3cda6969c1d55a38c88aebdd12bbb7ef3c9d333d8`.
This is a fresh audit artifact, not a release binary. Execution replay has its
own records below. Subsequent local documentation and test-driver additions,
including the strict-mirror registration change, are not part of that frozen
input; they do not change this build's production C++ translation units.

After the old build process group stopped, its 3,778 rebuildable BMI/object
files (11,134,431,391 bytes) were removed from the two exact `.gens`/`.objs`
directories under `ros-vendor23-modules`. Sources, LLVM archives, all build logs
and other agents' directories remain. Per-file hashes and exact targets are in
`ros-comment-retired-broken-bootstrap-artifacts.json`. These discarded cache
files are not backed up as this exact snapshot; regenerate them from the
recorded source/compiler/commands if needed. The earlier separate failed-BMI
backup remains untouched.

The completed entry-repair evidence (122 files) is preserved in
`ros-llvm23-entry-repair-evidence-20260916.tar.gz`, SHA-256
`83be45a4edd2e728282493aec908c2f4f8cd0fbafa39ee2bad71f2c317de1316`,
in the same Linux/Mac evidence locations. All member digests were checked and
the Mac archive digest matches. It includes failed attempt 02 and completed
preflights/header checks, but only attempt 03's input manifest. It excludes
active logs, VM binaries, BMIs and signing material.

The completed interpreter option results are separately preserved in
`ros-llvm23-int-profiles-evidence-20260916.tar.gz` (70 files), SHA-256
`261d507b330c99fe6aa4fd216e3f6324fc6eadd569e58938f4dddddd01df952f`,
in both evidence locations. It contains the failed initial setup, the 48 passing
semantic profiles, four passing execution profiles, exact commands and binary
digests, but not the executables themselves. Every member digest was checked
before copying, and the Mac archive digest matches.

After the entry-repair archive was verified on both machines, five completed
preflight dry-run logs (374,544,848 bytes) were removed to recover temporary
space. They remain recoverable from that archive under their original relative
paths. No source, BMI, result report, active build log or other agent's data was
removed; `ros-comment-retired-preflight-dryruns.json` records the exact files.

The module-object ownership follow-up is archived separately as
`ros-llvm23-module-ownership-evidence-20260916.tar.gz` (125 files), SHA-256
`6d69562463d9a25af674007684c4d0c599aa03902499f52043c3034d328348c3`,
in both evidence locations with matching archive digests and verified member
digests. It retains attempt 03's complete link failure, Linux/macOS fixture
results (including failed earlier fixture attempts), the frozen production and
later test-registration xmake inputs, and attempt 04's input manifest only.
It contains no active build log, binaries, BMIs or signing material. The updated
CI YAML and modified backend-guard shell block also pass parsing/syntax checks;
hosted CI has not been executed here.

The later bootstrap-compiler finding is preserved separately in
`ros-llvm23-bootstrap-guard-evidence-20260917.tar.gz` (54 files), SHA-256
`511222ca6a8f0f9ff507e036905b57ca1feb0e62d086f26c41b3fcd12d94aed5`.
Linux/Mac archive digests match, and every member digest was verified. This
archive includes the stopped attempt-04 log/result, rejected one-phase fixture,
Clang-22 semantic/ownership checks, exact cache-deletion inventory and completed
positive/negative bootstrap probes. Attempt 05 contributes only immutable
inputs and those closed probe logs, not its active build/configuration logs.

The ownership fixture later removed its incidental dependency on an unversioned
`clang` in PATH: a fixture-local xmake toolchain uses the supplied compiler
paths directly. All three controls pass on Linux with only `/usr/bin/clang-22`
and `clang++-22` selected (`ros-comment-module-ownership-clang22-paths/`), and on
macOS Clang 20 (`ros-comment-module-ownership-paths-mac/`). This test-driver
correction is outside attempt 05's frozen input and does not change VM code.

Seven historical ROS LLVM strict-mirror exclusions have been tested explicitly:
full Wasm 1.1, bulk memory, externref tables, table/ref bulk operations, basic
SIMD, if-without-else identity and validator alignment. All seven compile and
execute successfully through `UWVM2TEST_RUNNER_USE_LLVM_JIT`; records, exact
commands and executable digests are in `ros-comment-excluded-llvm-mirrors/`.
This reuses the recorded HEADER runtime object and pinned LLVM archives from
the earlier verified snapshot, without running llvm-config; it is not execution
of attempt 06's still-building module CLI. The obsolete exclusion list has
therefore been removed locally. The ordinary product already uses the complete
strict inventory. A negative registration control now catches restoration of
the old filtering pattern. This changes test registration, not production
execution or ROS's intentionally omitted lazy/tiered modes.

The closed mirror/launcher follow-up is archived as
`ros-llvm23-mirror-followup-evidence-20260917.tar.gz` (137 files), SHA-256
`7e04c52738ff74d9f6f83637d92e98e5a6cf6d97fe0fb9990d95afd00fd6c0b2`.
Every member digest was verified, and Linux/Mac archive digests match. It also
includes the original rejected-bootstrap IR, failed attempt-05 records,
include-flag scan controls and latest ownership/registration controls. It
excludes active attempt-06 outputs and all executables. After verification,
the seven completed mirror executables (546,969,368 bytes) were removed; logs,
commands, sources and digests remain. Their exact bytes are not backed up;
regenerate them with the recorded recipe. Exact deletion targets are recorded
in `ros-comment-retired-mirror-executables.json` on Linux and in the Mac
evidence directory.

Using attempt 06's newly bootstrapped LLVM archives, explicit audit-only
`llc`/`FileCheck` builds pass all ten retained MIPS RUN lines again
(`ros-comment-bootstrap22-mips/`). The tools' versions and digests are in
`ros-comment-bootstrap22-llvm-tools/`; neither llvm-config nor the metadata-only
consumer executable exists in that build. This is backend verifier/codegen
coverage, not a cross-platform ROS CLI pass.

The same new backend also passes the independent QEMU O32 matrix again:
12 BE/LE × O0/O1/O2/O3/Os/Oz runs, 2,409,600 exact-bit vector selections and
4,819,200 full-width f64 spill roundtrips. The deliberately unpatched backend
still reproduces the expected copy/spill failures as negative controls.
Four N32/N64 verifier/object checks pass but remain non-execution coverage.
Commands, tool/source digests, IR, assembly and guest objects are recorded in
`ros-comment-bootstrap22-mips-runtime/`. The unchanged frontend/integer-only
oracle isolates the newly bootstrapped backend; this is not a MIPS ROS CLI or
a new production-JIT-emitter test.

These completed MIPS results are preserved in
`ros-llvm23-bootstrap22-mips-evidence-20260917.tar.gz` (128 text files), SHA-256
`d2875b0ffda5b2a315f54660c157d5cf16c98039fcbab06cfeae6c0d2fc2e5a2`.
All members and matching Linux/Mac archive digests were verified. The archive
contains commands, logs, source fixtures, IR and assembly, not tool/guest
executables or active module-build output.

After selecting the checked bootstrap, Linux Clang 22.1.8 also passes all 48
interpreter compile-option profiles (combine 4 × delay-local 3 × reorder 2 ×
loop-unwind 2), with both tail and non-tail translator instantiations. Results
and exact commands are in `ros-comment-int-clang22-profiles/`; cumulative
compiler time is 2,155.98 seconds. This repeats the semantic matrix with the
new compiler, not just the old Clang-23 evidence. It remains a single native
ISA's header/template test, not 48 linked CLIs or all-instruction execution.

Four representative profiles also compile and execute successfully with
Clang 22: none/heavy/reorder-off, soft/heavy/reorder-on,
heavy/heavy/reorder-on and extra/heavy/reorder-on (loop unwind enabled).
Each executes four finite f64 subtraction cases through both tail and
non-tail runners: 32 value comparisons in total. The extra-heavy fixture
additionally requires the expected fused opcode. This is not NaN, signed-zero,
all-instruction or performance coverage, nor execution of all 48 profiles.
Exact recipes and results are in `ros-comment-int-profile-exec-clang22-01/`
through `-04/`.

The completed Clang-22 interpreter matrix, four executions and bootstrap
preflight are archived in
`ros-llvm23-bootstrap22-interpreter-evidence-20260917.tar.gz` (68 text files),
SHA-256 `9c16054db62bfbeb05ecf9c566f8b1d0dff515aed996ebd23a2638c1e5bdb8cf`.
Every member digest and the matching Linux/Mac archive digests were verified.
It contains no executables or active module-build output.

To reduce tmpfs memory pressure while attempt 06 continued, 3,305 unused
attempt-05 LLVM objects, archives, tools and its completion stamp were retired
(653,505,978 bytes). An open-file check rejected any in-use cache; all target
paths, sizes and digests were checked before unlinking. The earlier verified
header cache, active attempt-06 cache, sources and CMake/Ninja recipes remain.
The exact removed binary bytes are not backed up; they are rebuildable from
those recipes. The final inventory is `ros-comment-retired-unused-bootstrap-cache.json`
in both evidence locations. Its pre-deletion inventory and recipes are archived
as `ros-llvm23-unused-bootstrap-cache-evidence-20260917.tar.gz`, SHA-256
`78ecd33daf8ff4ba5c81d8c2cd21dbb51416ea7536e9ccf8036c7d6861515fae`,
with verified member digests and matching Linux/Mac copies.

## Completed fresh-module execution replay (September 17)

Attempt 06's exact binary above passes the following replay. Its driver first
requires the successful build result, unchanged source inventory and matching
binary digest, so an executable left by a failed build cannot satisfy the gate.

| Check | Result |
| --- | --- |
| Header/module CLI diagnostics | 20 exact output/exit-status pairs pass, with and without color |
| Actual interpreter add functions | 18 inspected scalar/SIMD functions pass: native add and tail dispatch, no calls or stack accesses in these selected functions |
| Core2 corpus | 2,296 ROS runs, 83,152 assertion executions, zero failures; the same 4,673 corpus exclusions remain explicit |
| Memory boundaries/grow | 652 real SIMD boundary/grow checks pass |
| Full feature integration | 65 feature/import-alias/DataCount checks pass |
| Unwind and authenticated cache | 108 CLI runs pass, including 72 signed-cache recursive trap replays |
| Mixed signedness conversion | 13 runs, 104 assertions and 6 authenticated replays pass |
| Native stack exhaustion | 150 runs across 15 cases and 10 profiles, including libunwind, pass |
| JIT generated code | The three inspected kernels and complete 1,574-byte text section match the header baseline byte-for-byte; the full generated object also matches |

Results are under `ros-comment-modules-*` in the Linux evidence directory.
The real dependency reuse/dry-run trace also passes: no llvm-config execution,
no llvm-config or metadata-consumer executable, all manifest/contract/path
guards pass, and the CLI digest stays unchanged (`ros-comment-final-build-guards/`).

The first 15-round, 10-million-iteration timing run measured interpreter
after/before = 1.04935 and JIT = 1.00017. It overlapped the build-guard check;
the header and module binaries also use different bootstrap compilers
(Clang 23 snapshot versus checked Clang 22.1.8). It is retained as an initial
observation, not evidence that the module fix caused a 4.9% regression or that
interpreter performance is unchanged. A quiet follow-up is recorded separately.
None of these native tests certify every ISA/ABI, every interpreter option,
Windows/Darwin OS integration, or all official stateful WAST scripts.

The quiet 31-round follow-up still measures interpreter after/before =
1.05786 and JIT = 1.00002 (`ros-comment-modules-performance-quiet/`). Thus
concurrent build checking does not explain the observed interpreter difference.
A same-source, same-Clang-22 header build was therefore constructed to separate
module-mode effects from the unmatched older baseline. The older binary also
comes from an earlier source snapshot; its comparison is not an isolated
measurement of the correctness fixes or solely of the compiler version.

The completed module build, replay, both initial timing runs and dependency
guards are archived as `ros-llvm23-modules-completed-evidence-20260917.tar.gz`
(4,587 text files), SHA-256
`31e12d2e7df1f20223b0dc8fdaf2a391741148d40110ef321a8c42b86b43ad9c`.
All member digests and matching Linux/Mac archive digests were verified.
The archive retains exact build commands, source inventory, generated WAT,
stdout/stderr, selected assembly and executable traces. It excludes binaries,
BMIs, runtime-cache contents and signing identities. The then-active matched
header build is not included.

### Matched Clang-22 header control

The matching header build passes after 300.03 seconds, reusing the exact
bundled LLVM cache (no LLVM library rebuild). Its source inventory remains
identical to attempt 06 and the module binary remains unchanged. Its SHA-256 is
`16be3b0a5ece2592fc38fa15db1432b83439a45c95b1bf1fe763046e193ea1f3`;
commands and results are in `ros-comment-matched-header22/`.

It independently passes the same 2,296 Core2 runs / 83,152 assertions, 652
memory checks, 65 integration checks, 108 unwind/cache runs, 13 conversion runs
/ 104 assertions / 6 authenticated replays, 150 exhaustion runs, and 18
selected real interpreter-function assembly checks (`ros-comment-header22-*`).

In the quiet 31-round, alternating-order, 10-million-iteration matched
comparison, module/header time is **0.95251 for the interpreter** and
**0.99996 for JIT**. The three JIT kernels, 1,574-byte text section and complete
generated object are identical between those two binaries. The module build
therefore does not show a slowdown against the matching header control in this
microbenchmark; it is about 4.7% faster here. This does **not** explain every
cause of the approximately 5.8% difference against the older Clang-23 binary,
prove a general module-mode speedup, or justify weakening a safety check.
Timing/assembly evidence is in `ros-comment-matched-header22-performance/`.

The matching build, replay and timing records are archived as
`ros-llvm23-matched-header22-evidence-20260917.tar.gz` (4,479 text files),
SHA-256 `d1f8657ce927b4f3fb9e78d7f3b23507bc20983a03e10d6b7d8b5bd94977c73d`.
Member digests and matching Linux/Mac archive digests were verified. As with the
module archive, binary/cache contents and signing identities are excluded.
