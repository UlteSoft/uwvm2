# Current macOS debugger acceptance: physical-budget candidate r2d

This is a new source candidate. The r1 source packet and its actual failed
entry record remain unchanged. No r2 component/native/product test has been
run by the author. Native execution belongs to the keeper; all product,
LLVM/SDK/MIG and Wasm fixture compilation stays in the admitted Linux 64 GiB,
no-swap cgroup. No local compiler or benchmark is authorized by this packet.

The actual r1 entry failed before any payload or orphan started at
RLIMIT_AS=(4GiB,4GiB), with original AS soft/hard both RLIM_INFINITY. Record
SHA256: 03135a5c0a36fdd27488c87e1b4cc6fa4d6eee2fabc9779df6821410f6e62844.
CPython maps every setrlimit EINVAL to “current limit exceeds maximum limit”;
this does not establish a privilege failure. Published XNU applies the AS
limit to vm_map size and can reject a limit below an existing mapped size.
The installed XNU release is not identical to that published source. The
actual result is preserved, not called a VM correctness failure or changed
to a claim about physical memory. No sudo, kernel setting or product mapping
guard change is requested.


## Preserved actual r2 exit-query failure and r2b correction

The keeper's actual r2 fixed true/orphan probe admitted the payload after
both fork and spawn returned EPERM in each owned participant. The probe
then failed in the unreaped root's BSD lookup; the original receipt remains
FAIL with root_reaped=false and group_retired=false. Its original receipt
SHA256 is af556a228119f18e90cb2edc180398977f8362e5c53cb2ccb7ce4ccd9750565c.
NAME/task_info was not reached, so that failure supplies no NAME permission
or maximum-RSS result. A subsequent keeper-owned read-only ps observation
found none of the three original PIDs; no later PID query reconstructs an
unrecorded original return value or authorizes signalling a reused PID.

Published XNU proc_info.c2075..2080 only enables zombie lookup for
PROC_PIDTBSDINFO when arg is nonzero. The r2 call used arg=0. The r2b candidate
passes arg=1 while retaining the complete 136-byte/PID result and every
existing birth/UID/PGID/held-child check. It also captures the original
retval/errno/returned PID/status in any future failed lookup diagnostic.
Unknown/partial results still fail; absent lookup remains possible only
when explicitly requested and the actual return is zero with ESRCH/ENOENT.
There is no waitid fallback, early reap, stale-birth substitution or sampled
maximum fallback. The next native action is the same fixed true/orphan probe
from the new owner SHA; a product still requires that probe to pass.

Three added synthetic controls exercise the nonzero zombie argument, exact
layout/PID enforcement, actual-result diagnostic fields and explicit absent
lookups through an owned fake output buffer. They do not call libproc,
Mach, a child process or a VM. The r2/r2a snapshots and native failure are
immutable; r2b is not labelled a native PASS before keeper execution.


## Preserved actual r2b teardown failure and r2c correction

The keeper's actual r2b fixed probe passed zombie BSD identity, actual orphan
reparenting and stopped-witness NAME accounting. The kernel returned a
witness lifetime resident_size_max of 27639808 bytes. It then failed during
anchored SIGKILL drain when PROC_PIDTASKINFO returned no task information
and the follow-up BSD condition did not qualify disappearance or the same
zombie. The original failure did not distinguish the follow-up status from
birth mismatch, or record task retval/errno; those values are not
reconstructed. The original receipt remains FAIL, SHA256
6ab52f7ecda776b823c7a006e5cfe91c413ab2edd68c9ed4c47e9057933db423, with root_reaped=false
and group_retired=false. Keeper's later read-only ps found no original
participants; no stale PID is signalled or relabelled as retired by r2c.

The r2c private retirement phase still requires the actual unreaped root,
complete group inventory and complete BSD Birth/UID/PGID checks, including
every known member. An unknown live group member is rejected. It does not
query withdrawn task RSS during kernel exit; its receipt explicitly marks
that retirement measurement unqualified and leaves RSS sample count/peak
unchanged. Normal admission/running RSS is strict, and future errors capture
actual task retval/errno and follow-up BSD status. Successful final retirement
still requires first actual wait4 root accounting, actual SELF peak, genuine
stopped-witness kernel peak, source closure and unchanged no-swap counters.
No missing sample becomes zero RSS or a substitute maximum. The exact fixed
true/orphan probe must pass from the new owner before any product executes.

Four new synthetic controls cover unavailable task RSS during retirement,
unchanged sample/peak counters, continued birth/group/unknown-live rejection,
strict normal RSS and preservation of original task errno across BSD retry.
They do not execute native APIs or payloads.


## Actual r2c PASS and corrected counter scope in r2d

The fixed true/no-fork/orphan r2c probe actually passed complete retirement.
Original receipt SHA256:
638f4a6876a2efc3888f1d216336ebdee4ab9fc4602c2ac5c3eb590dfd3e4f10.
Its sequential RSS observations peaked at 85000192 bytes, whereas reported
terminal counters sum to 58081280 bytes (SELF29065216, wait4 true1196032,
stopped witness27820032). These are different scopes. The old field named
kernel_peak_sum_upper_bytes is preserved in that immutable receipt, but
58081280 is NOT a full owned-PID/bootstrap lifetime upper bound.

Published XNU calcru assigns ru_maxrss from the current proc_task's
MACH_TASK_BASIC_INFO resident_size_max. That maximum belongs to that task's
phys_mem ledger. Exec can switch to a new task and terminate the old task
while retaining the PID birth. Our trusted guard exec chain is observed in
the RSS samples; its earlier task maxima are not all retained by final true
wait4. The installed kernel is not the identical published release, and no
unrecorded role-by-role old sample is inferred. Unique group PIDs plus the
separate supervisor PGID rule exclude intentional role double counting.

The r2d source labels the terminal sum kernel_reported_task_peak_sum_bytes,
retains at most16 real sequential role samples plus the actual largest
sample, and keeps each bounded role's actual sampled maximum with full Birth
identity. It rejects duplicate-role PID counting or changed Birth for an
existing role. max_of_recorded_budget_evidence_bytes is the maximum of actual
observed evidence and reported terminal counters, not a lifetime upper bound.
Running/admission observation still stops at512MiB, final counters must be
below4GiB, no-swap and full retirement remain mandatory. This is an observed
budget policy, not an aggregate kernel memory cap. Fixed product fixtures
have no imports and no process fork or later guest-driven exec; reported
product-task peaks cover that final task, with bootstrap observation separate.
No product has yet run, and r2d has only source/AST review by its author.

## Admission and accounting

macos_owned_debug_process_rss.py admits only fixed /usr/bin/true from its
standalone command line. The current product driver supplies at most seven
exact debug-jit case argv tuples after checking the keeper-pinned current
full product/official fixture packets. Each start checks the exact argv and
actual target SHA again. There is no arbitrary payload command-line option.
Only the fixed true probe may create the one pre-registered tiny orphan
witness before applying the sandbox. Fixed fixtures have no function imports;
the no-memory native/Wasm/replacement fixtures also have no linear memory.

The actual no-fork sandbox, nonce gate, birth (PID/time/UID/PGID), unreaped
session root, private stdin, regular-file output, no-swap counters and complete
post-exit group retirement are retained from r1. The supervisor plus complete
owned group is sampled every 20ms. An observed total at 512MiB stops the test,
leaving substantial headroom. This is an observation and early-stop policy,
not a kernel aggregate memory cap. AS is only read and recorded; there is no
4GiB virtual-map probe or an AS admission requirement.

After the true root has actually exited without being reaped, the witness
must really have the same birth and a changed parent. The unreaped owned root
anchors its group. killpg(anchor,SIGSTOP) stops that group; no potentially
reused individual PID is signalled. The witness must be in actual SZSTOP with
the same birth before and after querying a NAME-only Mach task port and
MACH_TASK_BASIC_INFO. That public flavor returns resident_size_max in bytes
from the kernel lifetime phys_mem accounting. The port is deallocated, the
anchored group is killed and fully drained, then the owner first reaps its
root. NAME has no guest/host memory-read or task-control authority. Refused
NAME/info/count/birth qualification is reported with the actual return code;
there is no sampled-peak fallback and no speculative sudo recommendation.

Each successful case requires

    root wait4 ru_maxrss + supervisor SELF ru_maxrss + stopped witness resident_size_max < 4GiB

The reported maximum of the stopped witness task is included even after its root
exits; it is not a maximum over earlier exec tasks. proc_pid_rusage lifetime_max_phys_footprint is a different metric and
is not relabelled maximum RSS. Product cases do not create descendants.
The final serial suite retains the largest reported final root/witness task
counters and supervisor SELF counter as a reported sum. Startup Python and
sandbox exec stages are observed under the same owned PID birth, but those
are distinct task lifetimes and are not all bounded by that final sum. Global
swap usage/swapped pages must stay zero, with cumulative swap-in/out counters
unchanged before, during and after. Sampling cannot establish a hard cap
between observations; the actual reported final-task counters must independently pass. Neither
they nor the sampled maxima are a hard full-PID lifetime upper bound.

## Actual source and loader closure

stage_macos_debug_current_product_rss.py retains the r1 complete actual
src/third-parties before==after fingerprint, fresh current main/runtime
objects, original direct compile/link/MIG commands, raw successful logs/tool
SHA, original input/output SHA, current joint producer/Stage3 pins, thin arm64
Mach-O and real DebugInfoDWARF archive proof.

The original build receipt purpose is now
actual-current-macos-arm64-debug-full-rss-build. It additionally supplies:

- actual_linker: {path,sha256}; original product-link argv must select that
  actual ld64.lld with -fuse-ld=/absolute/path.
- Original direct -isysroot/--sysroot/-syslibroot resolving to the actual MIG
  SDK and -F paths restricted to that SDK. Original -Wl,-t/-t log must contain
  the full actual selected Security/CoreFoundation stub paths as trace rows.
  A stub SHA or framework option alone is insufficient.
- macho_dylibs and macho_rpaths: actual original direct
  llvm-objdump --macho --dylibs-used PRODUCT and
  llvm-objdump --macho --rpaths PRODUCT records, each preserving cwd,
  returncode=0, input_sha256, tool_sha256, log and log_sha256.
- Actual main/runtime argv must agree in all non-identity macro definitions
  and undefinitions, including UWVM=2, UWVM_/UWVM2_, _LIBCPP_* and other
  namespaces. Actual header/framework searches, forced includes and SDK sysroot
  resolve against each original TU cwd and must agree. The compile SDK must be
  the actual MIG/link SDK. The exact arm64 Apple macOS/Darwin target,
  C++26/libc++, exception/unwind, packing, character-width and ABI flags
  must also agree. Only the four explicit non-layout identity macros are excluded in
  the parser, including actual UWVM2_BUILD_SOURCE_ID. The layout-affecting
  UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT macro must agree; capture enabled in
  only main or only runtime is a negative control. Duplicate/conflicting layout macros, unexpanded response files,
  shell/env wrappers, reconstructed commands and differing TU layouts fail.

This minimal loader closure requires absolute system /usr/lib dylibs and
/System/Library/Frameworks dependencies including actual libSystem, Security
and CoreFoundation. All LC_RPATH and non-system/@rpath dependencies are
unavailable in this lane until their exact current arm64 closure is qualified.
Actual load rows include weak/lazy/reexport/upward references from the LLVM
MachODump formatter. Tool/raw-log/product/source/SDK/archive pins are checked
again at the end. No DYLD_* override is permitted on Mac.

The qualification purpose is
actual-current-macos-arm64-debug-full-rss-qualified, and the Mac driver
requires its exact keeper-supplied SHA. This transports actual build proof,
not any debugger runtime permission; source/locals/native positions still
come exclusively from the real controller/runtime interfaces.

The r1 official fixture stager remains usable unchanged. It must actually
compile Clang C17 O1 DWARF5, link and validate Wasm, verify DWARF, retain the
real full line/inline oracle, reject actual imports, and officially encode
and validate the three fixed WATs. Synthetic tests are not official fixture
qualification. The product source/fixture/protocol helpers are pinned and
rechecked before/after. Source function-relative positions are also bounded
by their actual official Code expression extent.

## Keeper sequence

First run only the small ownership/no-fork/name-accounting/orphan probe:

    python3 test/0017.runtime/macos_owned_debug_process_rss.py \
      --out /tmp/NEW_PRIVATE_PROBE \
      --as-failure-receipt /tmp/uwvm-macos-owned-debug-process-entry-20261002-r1/entry-failure.json \
      --as-failure-sha256 03135a5c0a36fdd27488c87e1b4cc6fa4d6eee2fabc9779df6821410f6e62844 \
      --orphan-witness

The keeper captures original argv/rc/raw logs/source/tool SHA. Do not start
a product after failed birth, no-fork, NAME/info, swap or retirement proof.
No native execution is authorized by reading this document alone.

In Linux’s admitted cgroup, pure negatives may be run with:

    python3 -m unittest discover -s test/0017.runtime -p test_macos_debug_current_rss.py

Actual cross-build provenance qualification is:

    python3 test/0017.runtime/stage_macos_debug_current_product_rss.py \
      --source-root "$FRESH_SOURCE" --product "$ACTUAL_FRESH_ARM64_MACHO" \
      --build-receipt "$ACTUAL_ORIGINAL_BUILD_RECEIPT" --repository ros \
      --out "$NEW_QUALIFICATION_DIRECTORY"

Then use the current driver, supplying original packet SHA values:

    python3 test/0017.runtime/run_macos_debug_current_cli_rss.py \
      --source-root "$FRESH_MAC_SOURCE" --repository ros --product "$FRESH_PRODUCT" \
      --product-qualification "$PACKET/product.qualification.json" \
      --qualification-sha256 "$ACTUAL_QUALIFICATION_SHA" \
      --fixture-receipt "$OFFICIAL_FIXTURES/fixture.receipt.json" \
      --fixture-receipt-sha256 "$ACTUAL_FIXTURE_SHA" \
      --as-failure-receipt "$ACTUAL_R1_ENTRY_FAILURE" \
      --as-failure-sha256 03135a5c0a36fdd27488c87e1b4cc6fa4d6eee2fabc9779df6821410f6e62844 \
      --wasm "$OFFICIAL_FIXTURES/numeric.wasm" \
      --replace-wasm "$OFFICIAL_FIXTURES/replace.wasm" \
      --replacement-wasm "$OFFICIAL_FIXTURES/replacement.wasm" --out "$NEW_CASE_DIRECTORY"

Use --repository ordinary for uwvm2. The default seven cases are real Wasm
step plus two consecutive decoded Mach native instruction traps, malformed
then valid same-ABI replacement, C5 O1 physical source finish under instruction
and unwind strategies, and actual unsupported-mode fatal output. --subset
no-memory isolates four positive no-linear-memory cases plus the negative;
--subset source isolates two C5 finish cases plus the negative. The actual
orphan/name-peak probe always comes first. Only those named case scopes may
be declared passing; this packet does not claim all C/C++/Rust source into,
next/finish/variables, server, midrun attach, all-platform or benchmark PASS.

## Primary implementation evidence

- [XNU PROC_PIDTBSDINFO explicit zombie lookup](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/kern/proc_info.c#L2075-L2087)
- [Apple public mach_task_basic_info SDK source](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/osfmk/mach/task_info.h)
- [XNU task info lifetime resident and NAME conversion](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/osfmk/kern/task.c)
- [XNU task_name_for_pid same-user NAME policy](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/kern/kern_proc.c)
- [XNU RLIMIT_AS and getrusage maximum resident implementation](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/kern/kern_resource.c)
- [XNU exec task switching](https://github.com/apple-oss-distributions/xnu/blob/xnu-12377.121.6/bsd/kern/kern_exec.c#L5736-L5918)
- [CPython resource errno translation](https://github.com/python/cpython/blob/v3.14.0/Modules/resource.c)
- [LLVM MachO dylibs/rpaths formatter](https://github.com/llvm/llvm-project/blob/main/llvm/tools/llvm-objdump/MachODump.cpp)

Installed SDK layout and actual native accounting must still qualify; published
XNU source alone is not a claim that the installed kernel behaved identically.

The r2e source-only closure correction strengthens the existing Mac TU contract;
its three new synthetic negative controls are pending keeper execution. It does
not change the r2d owned process/RSS accounting, admit a new payload, or qualify
any product. Opaque frontend/preprocessor forwarding and precompiled modules
require separate provenance and are refused by this minimal direct-argv lane.
