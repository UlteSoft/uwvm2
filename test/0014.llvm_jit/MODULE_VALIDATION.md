# Named-module verification follow-up (2026-09-15)

This records build coverage, not whole-engine conformance or a proof of memory safety.

## Confirmed Darwin arm64 dependency defect

Both products' section_memory_manager.h included macho_headers.h in header mode,
but section_memory_manager.cppm omitted the same global-module-fragment include.
Its notifyObjectLoaded implementation uses the complete LLVM MachOObjectFile
type. A declaration imported from another module is not a substitute for this
owning partition's required definition.

The three-line, Apple/aarch64-guarded include is now present in both partitions.
No runtime body, JIT instruction, memory check, stack probe, or execution mode
changed. The repaired module file has SHA-256
68a0a7c791fa55fac039d4d182a2bd4ae6f82338fe16289a4555732a1c9d97a6.

check_section_memory_manager_module.py compiles the actual partition and ten
real dependency modules, without substituting textual stubs. With
--negative-macho-control, it also removes just this include from a temporary
copy and requires compilation to fail with a MachOObjectFile diagnostic.

Both ordinary and ROS passed on native Darwin arm64 with Homebrew LLVM 20.1.8
and the installed LLVM 23 development toolchain
004ffb73ee4c9b04407eae7c581a872ee328cc84. This is four successful 11-module
compilations (44 positive partition/module builds) and four expected negative
controls. It is not a complete Darwin CLI link/run. Both module-source checkers
also pass, as do their 39 checker-semantic tests. The shared-source parity guard
now covers 33 files and still checks four intentionally removed ROS mode paths.

Example (use a new output directory):

```sh
python3 test/0014.llvm_jit/check_section_memory_manager_module.py \
  --source-root "$PWD" \
  --cxx /opt/homebrew/opt/llvm/bin/clang++ \
  --llvm-config /opt/homebrew/opt/llvm/bin/llvm-config \
  --sdk /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  --out /tmp/uwvm-section-module-new --negative-macho-control
```

Local evidence is in /tmp/uwvm-paired-audit.cm4gqc/stack-performance:
section-module-{full,ros}-final{20,23}, each with commands, source hashes,
compiler logs, results.json and completed.json. The initial full-before-after-v3
trial also reproduces the original failure and successful candidate.
Earlier harness attempts missing the bizwen include path or inheriting LLVM's
-fno-exceptions failed before the target partition; they are not product failures
or successful coverage.

## Linux whole-product build failure and retry

The earlier Clang 22.1.8/default-libstdc++16 two-phase module build failed at the
command-line aggregate module, at approximately 40%, with
"ran out of source locations". No ROS build or whole-product replay followed.

Replaying that translation unit with -Rsloc-usage and -print-stats reproduced:

- 2,140,802,865 bytes of loaded source-location space, plus 24,808 local bytes.
- bits/version.h entered 9,129 times, using 874,147,395 source bytes plus
  25,067,277 macro-expansion bytes.
- The full build's maximum resident set was 11,777,300 KiB. It stopped on the
  source-location diagnostic, not a 32-GiB memory-limit failure. Historical
  child-cgroup OOM counters from earlier auxiliary attempts are not zero; they
  must not be confused with this build's failure reason.

Clang's [SourceLocation definition](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/include/clang/Basic/SourceLocation.h)
uses 32 bits with one macro-location bit; the
[AST reader allocation](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/clang/lib/Serialization/ASTReader.cpp)
reports this diagnostic when loaded source locations do not fit.

A real runtime_jit partition generated with --precompile,
-fmodules-reduced-bmi and a separate -fmodule-output produced a 42-MiB full BMI
and 3.2-MiB reduced BMI. Both serialized exactly 23,167 source-location entries
and 6,652,894 bytes of location space. Smaller BMI size alone does not solve
this failure. The newer documentation's --precompile-reduced-bmi spelling is
not accepted by this installed Clang 22; the documented two-output form was used.

The same partition's isolated global fragment consumes 6,649,647 location
bytes with libstdc++16 and 3,259,649 with libstdc++13. The new whole-product
attempt explicitly selects /usr/lib/gcc/x86_64-linux-gnu/13 while retaining
Clang 22, O3, two-phase modules, precise FP, native ISA flags, and existing
stack/linear-memory protection. This is a different standard-library-header
configuration, not a demonstrated fix for the original default configuration.

New fixed source snapshots: /var/tmp/uwvm-stack16.PsvyAm/module-g13-{full,ros}.
They include the later SIMD, stack/cache and module fixes; the ordinary manifest
SHA-256 is ca3dcd4356f4f47dc1361dc7d6a56d05b3464dff765cf0126ecd85718ecaaacc.
At this update the ordinary build is running, ROS is queued, and neither build
nor their queued runtime replay is counted as a pass. The replay requires both
links before testing exhaustion, directed regressions, start ordering,
integration, SIMD boundaries and actual signed-cache SIMD assembly.

Later in this attempt, the ordinary uwvm target successfully produced the
previously failing uwvm2.uwvm.cmdline aggregate BMI and continued to later
partitions. Its 51,964-byte output was also parsed successfully by the matching
llvm-bcanalyzer; SHA-256:
f84e97806d75bc796f5c77b41da936de8b95caadf71486ed82360f680b692515.
This crosses that specific earlier failure point with GCC 13 headers. It is
not a complete build/link, a fix for the default GCC 16 header configuration,
or a ROS build pass. Evidence: cmdline-g13-bmi-bitstream.log under the remote
scratch root and its local module-followup32 evidence directory.

The entire Linux workflow remains in uwvmsimd-zHWxwM.slice with MemoryMax=32GiB,
swap disabled, and affinity 16-31. BMI concurrency is two; backend code generation
uses an exclusive compiler lock. Compilers stop below 12GiB disk headroom.
The failed generated BMI directory was archived, byte-compared against the
archive, and then removed, releasing about 9GiB net disk space. Sources, logs,
manifests and earlier binaries remain. Recoverable archive:
module-full-sloc-failure-bmis.tar.zst (SHA-256
64947d6f7d02a39daac648554ddfee6bc58ec596d727850e0e0a994f9ea50771).

## Existing Linux SIMD BMI versus header code generation

check_existing_simd_module_codegen.py reads the ordinary build's actual SIMD
BMI and its recorded xmake compiler flags, without changing the source or BMI
tree. It compiles eight shared-evaluator rounding wrappers in both header and
named-module mode. On Linux x86_64 with Clang 22.1.8/GCC 13 headers, all eight
instruction/operand sequences match: one vroundps/vroundpd, one vector result
store and retq. No native helper call or stack access appears in those wrappers.
This is not linked module-runtime execution, an engine JIT object, a throughput
measurement, or evidence for the not-yet-built ROS BMI.

The production BMI SHA-256 before and after is
444206edbc75dad95dbc421d1ee151c78cc48581e7aba0d4d1814858ae98e821.
The conditional-import fixture SHA-256 is
3b6f19d9d495bc0976a532fce20a590c9301c2ea95589f48b1aaf523312ae28c.
The permanent driver passed separately after the initial prototype.
The queued ROS build wrapper now runs the same comparison before generated
BMIs are archived. The already-running ordinary build keeps its original
wrapper inode and separate completed comparison. No active source snapshot
or manifest was changed; the queued ROS check is not yet a pass.
Evidence: /var/tmp/uwvm-stack16.PsvyAm/simd-module-codegen-g13-final and
/tmp/uwvm-paired-audit.cm4gqc/evidence/module-followup32/simd-module-codegen-g13-final.

## Expanded paired-memory guard

The shared-source checker now covers 49 files, adding the interpreter memory
header/partition and the complete object/memory/linear tree. Only the exact
JIT-versus-LLVM-AOT terminology comment is normalized in memory.h; every
executable line is still compared. The existing explicit tiered-output
difference remains allowed, and all four removed ROS mode paths remain absent.
Both copies pass the real paired-tree check and nine synthetic checker tests
each, including rejected memory-code drift, missing allocation files and
accidentally restored ROS modes. This is synchronization coverage, not semantic
equivalence of all product code.

The independent address-decision oracle now also runs before and after O3;
paired results and negative controls are recorded in SIMD_DIRECT.md. These
test-only changes are outside the fixed, active whole-product build snapshots.
