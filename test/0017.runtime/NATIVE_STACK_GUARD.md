# Native stack protection: entry-cost optimization

This changes native-stack fault setup, not Wasm call-stack tracking, stack
canaries, or linear-memory bounds checks. Ordinary and ROS use the same header;
ROS retains only its supported full execution modes.

## Strategy

- Reuse a guarded 128 KiB alternate signal-stack allocation per OS thread.
  Each outer entry still queries the host's current alternate stack and restores
  it on return. An already adequate host stack is borrowed unchanged.
- Cache Linux pthread stack bounds. Fixed, explicitly guarded pthread stacks
  need no repeated limit query. Main/zero-guard stacks retain a live
  `getrlimit(RLIMIT_STACK)` check and refresh attributes when the limit changes.
  The actual entry address must be inside the cached extent.
- Darwin retains its native pthread bounds queries. Cache the immutable OS page
  size. Nested VM entries share the outer scope and perform no OS setup calls.
- Keep signal-visible bounds in a lock-free atomic pointer to the active scope,
  not in the mutable cache. Thread-specific teardown restores ownership before
  releasing memory. Key/allocation failure falls back to an uncached mapping;
  setup failure does not run unprotected Wasm.
- Keep ISA-specific compiler probing: supported non-Windows x86/AArch64/SystemZ
  LLVM backends default to 4096-byte inline probes; supported RISC-V
  uses 2048-byte probes because a split register-save adjustment previously
  allowed a 4096-byte probe to skip a 4 KiB guard. Windows lowering is not
  overridden. Unsupported backends are not claimed to acquire protection merely
  by accepting an ignored attribute.
- Native Apple arm64 JIT may use 16384-byte probes for macOS targets only after
  confirming a 16384-byte OS page. iOS, non-Mac targets, unknown page sizes and
  other build hosts retain the conservative interval. This page query happens
  during IR emission, not in generated Wasm. Native C++ build flags stay unchanged.

The cached allocation has a metadata page, an inaccessible page, the usable
signal stack, and another inaccessible page. Reuse removes repeated allocation,
protection changes, and main-thread `/proc/self/maps` parsing from hot entries.
It does not remove the guard or probing that makes stack exhaustion detectable.

The local WAVM comparison found thread-lifetime signal-stack initialization in
[ThreadPOSIX.cpp](../../../WAVM/Lib/Platform/POSIX/ThreadPOSIX.cpp),
signal handling in
[SignalPOSIX.cpp](../../../WAVM/Lib/Platform/POSIX/SignalPOSIX.cpp), and target-gated
`wavm_probe_stack` attributes in
[LLVMJITPrivate.h](../../../WAVM/Lib/LLVMJIT/LLVMJITPrivate.h).
WAVM does not treat libunwind as a substitute for all stack-fault machinery.

## Measurements

Same opt-in [benchmark](benchmarks/native_stack_entry.cc), compiled with O3;
five samples, 10,000 outer entries/sample and 1,000,000 nested entries/sample.
Linux used Clang 22 on CPU 31, within the existing CPUs 16–31 / 64 GiB aggregate
test cap. Darwin used Homebrew LLVM on the local arm64 host.
These are thread CPU-time medians, not wall-clock VM throughput. Background
builds were present, so differences of a few nanoseconds should not be ranked.

| Host/path | Previous setup (ns/entry) | Cached setup (ns/entry) |
| --- | ---: | ---: |
| Linux x86_64 main | 54,198.90 | 692.11 |
| Linux x86_64 pthread | 5,191.38 | 508.51 |
| Linux x86_64 adequate host altstack | 49,139.15 | 367.75 |
| Linux x86_64 nested | 5.81 | 5.36 |
| Darwin arm64 main | 1,254.52 | 570.17 |
| Darwin arm64 pthread | 1,041.95 | 317.82 |
| Darwin arm64 adequate host altstack | 89.63 | 96.95 |
| Darwin arm64 nested | 2.45 | 3.00 |

A post-reboot rerun compares the same pre-cache baseline against the later
d7dd42a0 signal/cache header under the 32 GiB aggregate cap, on CPU 31 with
Clang 22 and concurrent module-build activity. Thread CPU-time medians were:
main 40,665.16 -> 684.06 ns; pthread 4,484.77 -> 505.40 ns; adequate host
altstack 36,989.61 -> 348.30 ns; nested 5.40 -> 5.205 ns. Small nested differences
are not a meaningful speed ranking. The last path combines ten samples from
main and pthread entries. Raw samples are retained in
/tmp/uwvm-paired-audit.cm4gqc/evidence/stack32/benchmark-current.

The Linux short traced run (500 entries in each outer scenario, plus nested
loops and initialization) reduced mmap calls from 1028 to 26, munmap from 1007
to 4, and reads from 6016 to 10. Sigaltstack calls remain approximately unchanged
because host state is still queried and restored. These counts include process
startup and are not counts for one VM call.

Local raw evidence: `/tmp/uwvm-paired-audit.cm4gqc/stack-performance`.
Remote raw evidence: `/tmp/uwvm-paired-closure.W9Xx4o/stack-performance`.
The previous header comes from the fixed revision5 product snapshot.

### Native Apple arm64 large-frame specialization

Both products' production attribute emitters now pass
[check_darwin_stack_probes.py](../0014.llvm_jit/check_darwin_stack_probes.py)
on the native 16-KiB-page Mac with LLVM 20.1.8. Each run tests 18 frame sizes
from 1 to 131072 bytes, 14716 synthetic guard-entry alignments, 18 ordinary
recursive exhaustion cases, and six target-policy selections. Both products
pass, including the fallback for iOS/Linux and the Windows exclusion.
The synthetic cases place writable memory below the protected page so that
skipping it cannot masquerade as an ordinary exhaustion fault.

The actual 20000-byte production frame has two probe writes instead of five;
the small function remains a bare ret. This is an instruction-count result,
not a whole-VM timing claim. Before changing production, the same 18-size
prototype also passed with LLVM 20 and LLVM 22 objects on the Mac. Prototype
results are not substituted for the actual-production runs.

Both actual-production suites also pass with the installed LLVM 23.0.0git
toolchain (004ffb73ee4c9b04407eae7c581a872ee328cc84). That development backend
uses two LDR-to-XZR probes instead of STR-to-XZR; the test recognizes either
instruction but still requires every actual PROT_NONE guard test to pass.
There are 58864 guard-entry checks and 72 recursive cases across the two
products and these two native compiler versions. This does not establish
coverage of every intervening LLVM release or physical CPU.

The LLVM 23 attempts first failed due to its missing default SDK search path
and mismatched linker/LTO settings. The final run passes explicit SDK, matching
ld64.lld, and macOS platform-version options through --cxxflag. These are local
test-command settings, not edits to the installed toolchain. An intermediate
test then rejected LLVM 23's LDR spelling; its partial run remains recorded as
failed, not as a pass. Final evidence: page16-production-{full,ros}23-final.

On Linux/LLVM 22, the updated and fixed-snapshot emitters produce byte-identical
IR for six targets in both products (12 comparisons). The first attempt lacked
its scratch directories; the next exceeded an auxiliary 1-GiB child limit.
Both failed attempts are retained. The completed retry uses a 3-GiB child limit
inside the same aggregate 32-GiB slice. The parent limit itself has no OOM/max
event; the hierarchical OOM count includes that one small-child failure.
Evidence: evidence/stack32/page16-linux-check-v3.log.

The emitted probe-size attribute participates in the existing full-module IR
hash (including parallel-object keys) and normal object-cache bitcode hash.
This optimization changes no Wasm memory guard, call tracking, or ROS execution
mode. Raw evidence is in /tmp/uwvm-paired-audit.cm4gqc/stack-performance:
page16-production-{full,ros}, page16-llvm20-matrix and page16-llvm22-matrix.
These are native generated-code tests, not new complete Darwin CLI builds.

## Validation and remaining work

### Post-reboot continuation

The user changed the task's aggregate Linux cap to 16 GiB, then to **32 GiB**.
The current persistent slice limit is 34,359,738,368 bytes, with swap disabled;
it covers all child jobs, not 32 GiB per process. Heavy jobs share a serial lock
and have a 28 GiB child limit. Builds use one compiler process; inherited CPU
affinity is checked as CPUs 16–31. The host does not delegate the cpuset
controller to user services, so affinity is established with taskset.

The reboot cleared the old remote /tmp workspace. Restored sources, private
toolchains and new outputs live on disk at /var/tmp/uwvm-stack16.PsvyAm (the
directory's historical name is not the current cap). Linux x86_64 fixed-snapshot
tests now pass on both products with GCC, Clang and UBSan: six runs, including
concurrent first entry and the uncached exhaustion path. Copies of these logs
are in /tmp/uwvm-paired-audit.cm4gqc/evidence/stack16 on the Mac.

An initial monolithic product compilation was stopped near the earlier memory
budget and is not a pass. The retry retains O3/target/FP/probing options and uses
Clang's clear-ast-before-backend option to reduce overlapping frontend/backend
memory. Both non-module products subsequently compiled and linked successfully.
The first links failed because the restored private LLVM toolchain lacked
development-library entries and retained stale component detection; the final
links used the restored shared LLVM 22 after rechecking detection. These failed
attempts are not successful builds, and final-link timings are not full-build
timings.

The restored GCC 15/QEMU matrix passes for i686, AArch64, ARM, PPC64 BE/LE,
RISC-V64 and s390x. Each guest explicitly skips live RLIMIT mutation because
QEMU reports an unchanged limit, while still testing cache invalidation. Native
i686 also passes, including actual live-limit mutation. These are native guard
unit tests, not whole-JIT execution on all seven architectures.

The fixed products pass 285 exhaustion replays, 108 directed Wasm regressions,
98 start-order runs, and 65 integration checks per product. Both pass 652 SIMD
boundary/growth checks each. Actual production-cache objects have identical
SIMD hot loops: four vmulps, four vaddps, no call and no stack access. The
partial two-worker boundary attempt was intentionally stopped and is not a
pass; the completed boundary runs use eight workers under the same cap.

A later two-process cache replay on these same x86_64 binaries explicitly
reports object-cache-hit for both products, with unchanged signed cache files.
Decoded hot loops remain identical (four vmulps/four vaddps, no calls or stack
accesses). Evidence: evidence/stack32/cache-hit-replay. This remains validation
of the non-module snapshots identified below, not of unfinished module builds.

Tested source-manifest SHA-256:

- Ordinary: 19f6f8136dd147ccbc148b86fd0cd5b20c529e9691c8a894ec778325026b86e8
- ROS: 65a9526cd767e08a8c3b393fec4a1ce7334dc3121614b0365e21cee556e4d6e2

Both snapshots use native-stack header SHA-256
28e12b7dafb84cdc5fbcbb010440f7aee54b50fd7aca4e8b5007312c3d61ae09.
A concurrent local signal-installation/forwarding change appeared during
these builds. It was preserved, not copied into a running build. Its later
header SHA-256 d7dd42a067ed7d8faf064453c5918aa36af3ef2b6f3ff41804bca15fffb9170c
separately passes all six Linux guard/cache runs, the seven QEMU architectures
and native i686, plus both products' GCC/Clang O0/O3 signal-installation suites.
The earlier whole-product replay still belongs to the older header, not to
this later signal-handling snapshot.

Complete named-module products are rebuilding from fixed snapshots containing
the later signal header and the RISC-V address follow-up in SIMD_DIRECT.md.
These fixed Linux snapshots predate the subsequent Apple-only page-size
specialization above; running builds are not modified in place.
Before ROS's first build/manifest, a later paired-source check found its shared
SIMD rounding helpers had not received ordinary-product updates. The unstarted
ROS snapshot was synchronized with the frozen ordinary snapshot and both now
pass the 29-file shared-source parity guard. The subsequent local GCC-only
packed-rounding optimization is separate from these Clang module builds;
its actual native/cross tests are recorded in SIMD_AUDIT.md.
The old failed module intermediates were archived and compared byte-for-byte
before removing their six .deps/.gens/.objs directories. The 11.6 GiB archive
is /var/tmp/uwvm-stack16.PsvyAm/old-module-intermediates.tar.zst; SHA-256 is
282782f6b3530bdca90d6f36ce506c6b9488d8e33f3e599238e4354ea1dc9a75.
It can restore those generated files. New compilers stop if disk headroom
falls below 12 GiB; O3 and the aggregate 32 GiB memory limit remain unchanged.

Selected logs are saved locally in evidence/stack32 and evidence/stack16.

### Earlier evidence

Completed for this cached implementation:

- Linux x86_64 GCC guard/cache tests before the final test-only
  additions: cold/warm recursion, main/pthread stacks, nested scopes, host-stack
  restoration, key-exhaustion fallback, thread cleanup, late destructor re-entry,
  and live RLIMIT refresh. The current header also compiled and executed the
  Linux Clang entry benchmark; the post-reboot six-run matrix above supersedes
  the earlier pending final Clang fault suite for its identified snapshot.
- Final Darwin arm64 test source on both ordinary and ROS headers; the ordinary
  run also passed UBSan. It includes eight simultaneous first entries, an actual
  exhaustion fault on the uncached/key-exhausted path, small and large host
  signal stacks, ownership/restoration, and thread cleanup.
- An LLVM-generated 20,000-byte arm64 Mach-O frame linked to the new guard
  correctly faults and exits with the native exhaustion diagnostic. Its small
  frame is a bare `ret`; the large frame has inline probes, not a probe helper.

At disconnection, not completed: final Linux/QEMU matrix, revision6 whole-product builds/replays,
and the named-module product builds. The Linux host went offline during this
work. Do not reuse revision5's successful 285 exhaustion replays or broader
Wasm matrices as validation of the new cached entry implementation.

A QEMU i686 run exposed a test-environment distinction: setrlimit accepted a
new stack limit, but getrlimit still reported the old guest stack size. The
fixture now explicitly reports that live-limit case as skipped under the
QEMU-only test define and still exercises cache-tag invalidation. Native Linux
must test the real limit change. This adapted guest test was not completed
before disconnection.

Darwin teardown tests use the actual Mach mapping query, not an assumption
that mincore returns ENOMEM for an unmapped page. The observed residency-only
behavior is consistent with [Apple's XNU implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_mman.c).

## Boundaries

This is not a proof of universal native-stack safety. Guardless/custom/fiber
stacks, arbitrary host remapping of the native stack, signal-handler replacement
during VM execution, and unloading the runtime while threads still own its
thread-specific state are not covered. Existing unsupported OS/ISA handling
remains unsupported. No new per-Wasm-call depth counter or linear-memory bridge
was added by this optimization. The ISA32/ISA64 linear-memory access strategy is
unchanged.
