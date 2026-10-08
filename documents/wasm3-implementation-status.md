# WebAssembly 3.0 implementation status

This document is a chronological implementation and qualification record for
both repositories. Early sections describe the state at their named revision;
the latest section at the end gives the current source IDs, supported behavior,
test coverage, and remaining acceptance limits. Passing focused tests do not
by themselves establish full standard conformance or full-platform acceptance.

## Normative references

- [Core 3.0 change history](https://webassembly.github.io/spec/core/appendix/changes.html)
- [Constant-expression validation](https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions)
- [Table binary grammar](https://webassembly.github.io/spec/core/binary/modules.html#table-section)
- [Table validation context](https://webassembly.github.io/spec/core/valid/modules.html#tables)
- [Core 3 numeric semantics](https://webassembly.github.io/spec/core/exec/numerics.html)
- [Core 3 memory instruction execution](https://webassembly.github.io/spec/core/exec/instructions.html#memory-instructions)
- [Relaxed SIMD specification and tests](https://github.com/WebAssembly/relaxed-simd)
- [Core specification tests](https://github.com/WebAssembly/spec/tree/main/test/core)
- [Threads proposal](https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md)
- [LLVM exception handling](https://llvm.org/docs/ExceptionHandling.html)

## Initial feature slices in both repositories (historical)

The limits in this table describe the first implementation of each switch.
Later dated qualification sections supersede them; they are not a current
capability list. In particular, GC constant expressions, typed references,
`call_ref`, and LLVM-full function replacement have since advanced. Refer to
the latest qualification section for the current status.

Each addition has separate enable/disable switches. The Core additions below and the partial tail-call implementation are initially
disabled. Long forms are `--wasm-feature-enable-NAME` and
`--wasm-feature-disable-NAME`; aliases are `-WFE-NAME` and `-WFD-NAME`.
As with existing scoped feature switches, they conflict with complete version
selectors. MVP, Wasm 1.1, and Wasm 2 selectors keep these additions disabled.
There is no complete Wasm 3 selector yet.

| NAME | Implemented behavior | Limits |
| --- | --- | --- |
| `extended-const` | i32/i64 add, subtract, multiply; previous immutable local globals; global and active-segment expressions | GC constant instructions and typed references remain unavailable |
| `table-initializer` | Definition-only `0x40 0x00` prefix, nullable funcref/externref initializers, imported references, declared `ref.func` collection | Non-nullable/typed table references require the missing type-system work |
| `relaxed-simd` | All 20 opcodes `0xfd 0x100` through `0xfd 0x113`, standalone and integrated validation, interpreter and native LLVM lowering | Also requires SIMD; remaining Core 3 features are independent |
| `memory64` | Full-width binary memory limits and offsets; i64 addresses, size/grow, scalar/SIMD/bulk/atomic translation | Focused qualification below; complete mode/platform acceptance remains outstanding |
| `table64` | Full-width local/import table limits, i64 active offsets, typed table operations and indirect selectors | Interpreter and native JIT focused tests pass; full mode/platform/cache qualification remains in progress |
| `multi-memory` | Indexed scalar/SIMD memargs; indexed size/grow/init/fill; independent source/destination for copy; native and imported/provider memories | Memory64 has its own independent switch |
| `exceptions` | Tag instance linking; numeric cross-function `throw` / `try_table` with ordered `catch` / `catch_all` in interpreter and LLVM full/lazy; focused tiered execution; standalone Wasm 3 typing for `ref.null exn`, `throw_ref`, `catch_ref`, and `catch_all_ref` | Reachable exception-reference execution, retained reference payload ABI and remaining Core 3 EH integration are unfinished |
| `function-references` | `ref.as_non_null`, `br_on_null`, `br_on_non_null` on integrated funcref/externref carriers | Rich declarations and call_ref remain unfinished; this is not the complete proposal |

Extended integer expressions use a checked growable opcode vector and typed stack
depth validation. Self/forward global references, mutable sources, incorrect types,
underflow, surplus operands, and truncated encodings fail validation. Initialization
waits for imported globals, evaluates integer arithmetic modulo 2^32/2^64, preserves
floating-point bits and destination mutability, and checks active segment bounds
after evaluating offsets. Feature policy is rechecked on already-parsed modules.

Table imports retain their old type-only grammar. Explicit table initialization
uses the imported-global-only validation context, evaluates a reference once, and
fills the table before active element segments. Imported function references retain
the provider's identity. Parallel initializer storage leaves the public table-type
layout intact; runtime local-table storage changed. The historical v20 cache ABI added `local-table-layout=initializer-expression-v1` and
`native-memory-layout=bounded-reservation-aligned64-owner-ordered-size-v4`.
The r50 v18 cache test passed 48 serializer/signature checks, including v16/v17 rejection.
The v19 bump additionally invalidates ambiguous atomic bridge identities; r56 passed
52 serializer/signature checks including v18 rejection. A separate native-memory-backend
field now distinguishes mmap, concurrent allocator and single-thread allocator builds;
r60 passed 21 configuration-isolation invocations per product across all three builds and all six replay directions.
The r60 product-isolation rerun passed all 52 invocations. v20 additionally invalidates
the old narrow atomic bridge ABI; r61 passed the updated 56 product/schema checks, including v19 rejection, and 21 memory-backend checks per product.
The mmap record keeps its original hot member offsets, places the bounded-reservation
extent after them, and aligns each instance to 64 bytes.

Relaxed SIMD uses a fixed allowed projection: strict swizzle/conversions/min/max/
q15, bitwise lane selection, unfused multiply/add, and signed saturating pair sums
for dot products. LLVM lowers operations to vector IR without an interpreter bridge.
The register-ring ternary handlers preserve `UWVM_MUSTTAIL` dispatch. The ordinary
lazy structural scanner recognizes the new immediate-free opcodes. The shared
policy helpers live in `src/uwvm2/validation/standard/wasm3/`; this directory is
**not yet a complete Core 3 validator**.

The Wasm 3 standalone validator now contains a full copy of the Wasm 2 validator
body, with the Wasm 3 tag, policy dispatch and indexed-memory validation. Both
repositories use that complete body; ROS no longer inherits only its short Wasm 2
adapter. Runtime, lazy, interpreter and LLVM validation route through the new
policy entry. This is a working base with the additions listed above, not complete
Core 3 type-system support.

The complete `wasm1p1/` and `wasm2/` validator directories have been restored
to repository HEAD. Pure validator additions reside in `wasm3/` only. Its
`impl.h` includes every helper and validator header; `impl.cppm` exports their
actual named modules and the validator partition. The module dependency checker
now distinguishes a local named helper module from a partition. Both complete
header/module dependency scans and all 45 checker unit tests pass. After the
restoration, both r8 parser/initializer, multi-memory and relaxed-SIMD focused
suites pass; previous full-CLI evidence below retains its original fingerprint.

Multi-memory uses transactional bounded immediate decoding: memarg flag bit 6,
u32 memory indices including multi-byte and padded encodings, a u64 binary offset
with the memory32 range restriction, and indexed bulk operations. The interpreter
selects native memory objects during translation; the ring handlers carry their
actual pointers without a runtime index lookup. Fusion cannot reuse a pending
load from a different memory. Copies check both complete ranges before writes,
coalesce aliases and acquire locks in a stable object-address order. LLVM emits
distinct native memory symbols and supports mixed provider/native copies, including
overlap through imported aliases and copies spanning its staging buffer.

On 32-bit hosts, a memory with a small declared maximum reserves only its bounded
extent instead of consuming 256 MiB per instance. These bounded reservations use
software bounds checks, including both growth APIs; 64-bit full guards retain
their existing access path. Page-fault registration carries the actual owning
memory index. A new unit executes 130 instances, growth/move/reset, and indexed
fault diagnostics on native Linux, ARM32 and i386 under QEMU.

The ordinary runtime now preserves lazy background-validation errors for demand
callers that observe a terminal failed compilation. New malformed multi-memory
cases exposed the previous missing diagnostic and the misleading success error.

New decoding cursor and interpreter stack/IP updates have bounds and lifetime
comments adjacent to the updates. Existing instruction paths are reused where the
selected relaxed projection is exactly an already-implemented strict operation.

## Threads development slice (r20–r36)

The separate opt-in `--wasm-feature-enable-threads` / disable switch currently
implements `atomic.fence`, seven atomic loads and seven atomic stores;
49 RMW variants now have focused validation/execution evidence; it is
**not the complete threads extension**. Core 3's
implicit feature set does not enable this extension. Its bounded decoder accepts
padded u32 subopcodes but requires a single literal zero reserved byte, commits the
cursor transactionally, and allows a module with no memory. Standalone validation,
full interpreter/JIT compilation, and the ordinary lazy structural scanner share
this decoder. All pure-validator changes remain under `validation/standard/wasm3`;
`wasm1p1` and `wasm2` validator directories remain identical to HEAD.

The interpreter fence keeps cached register-ring values in place, flushes pending
instruction combinations through the existing dispatch gate, and musttail-dispatches
to its successor. LLVM emits an actual sequentially consistent fence, without a
runtime bridge. Both products passed 44 cases in each of uncached interpreter,
register-ring interpreter, and full LLVM execution/IR checking on SSH Linux. The
interpreter development configuration uses extra-heavy combine and heavy delay.
Ordinary lazy immediate scanning is checked by the same fixture. This is not a
complete lazy/tiered execution or cross-platform qualification.

`utils/thread/keyed_wait_set` provides owner-scoped, keyed waiting with exact wake
counts, monotonic bounded timeouts, race-free compare/enqueue, cancellation and
drain. `utils/thread/execution_lifetime` supplies bounded admission, cooperative
stop and move-only lifetime leases for host-owned execution threads. Neither
contains Wasm-specific types. Both products' focused ASan/UBSan tests passed exact
wake count, resource/position isolation, immediate timeout, compare/enqueue races,
saturated timeouts and stop/wake/drain ordering. Native OS threads remain owned by
the caller and must be joined. VM entry, stop and teardown now use execution
generations as described below. The later r39–r55 work below connects shared memory and guest waiting; its remaining qualification is explicit.

A cold-only native memory owner index now makes software access failures report
the same owner as the hardware fault registry. Imported aliases retain owner
identity. The field follows hot metadata, and successful accesses do not read it.
The old r19 PPC32 run reproduced the memory-1-reported-as-0 failure; both r20
PPC32 uncached store/growth reruns passed. The v16 cache-isolation fixture passed
40 signed/unsigned serializer checks, including v15 context rejection in both products.

Remote artifacts: `/work/artifacts/threads-r20/{uwvm2,uwvm2-ros}`;
`/work/wait-r23/{uwvm2,uwvm2-ros}`; `/work/artifacts/memory-owner-r20`.
The interpreter binaries' actual native fence handlers contain a locked stack OR
(the compiler's sequentially consistent x86 lowering), no calls, and an indirect
tail jump in register-ring mode. The required guest fence is separate from ordinary
memory access: it does not add fences to unrelated load/store handlers.

The subsequent r23 primitives test also passed per-owner cancellation on a shared
wait domain, an already-stopped token, and stop-vs-enqueue races. Drain covers stop
callback destruction as well as queue removal. The r24 common atomic decoder now
describes all 67 valid FE opcodes, exact natural alignment, width/stack metadata,
and multi-memory immediates; its 76 focused family/width/index/padded variants and
truncation/error cases passed ASan/UBSan in both products. This decoder is preparation
for the remaining execution work. The executable subset now accepts the fence
and seven load/store variants each. All 49 RMW variants also have focused execution evidence.
Shared-memory and waiting additions follow in the current development section below.
The official threads `atomic.wast` memoryless-fence case and the live-operand fence
fixture passed the available Wasmtime 42.0.1 reference. No claim is made that the
entire official atomic suite has passed UWVM.

New utility/module surfaces passed the module import/export audit in both products
(`/work/audit-r22`). Full threads conformance and the complete final matrix remain
outstanding. At that stage, unsupported atomic opcodes failed validation; the guest wait/notify additions are described below.

### Atomic load qualification (r27–r29)

Both repositories passed 80 binary cases (28 accepted, 52 rejected) in uncached,
merged register-ring and separate register-ring interpreters, with extra-heavy
combine and heavy delay enabled. All seven opcodes run against memory 0 and
indexed memory 1, with live results across consecutive loads, high-bit zero
extension, the last valid page access, wrong operand types, over/under alignment,
polymorphic unreachable code and explicit feature rejection. Real executions trap
on misalignment, page bounds and a u33 address-plus-offset overflow.

Full LLVM passed the same typed cases and trap diagnostics with both instruction
and native-unwind stack recording. The actual 28 translated modules per product
retain two sequentially consistent loads with natural alignment in optimized IR.
Their native assembly has no added fences, atomic helper calls, or instruction
stack callbacks on the unwind success path. Every one of 108 instantiated atomic
interpreter tail handlers has an indirect successor jump; its straight-line
success path has no calls or fences. Cold trap paths are checked separately.
Comparing actual r20 and r28 binaries found identical successful instruction
sequences in all 136 common ordinary full-mmap load/store handlers per product.
This is a bounded assembly check, not a complete throughput qualification.

Actual new-syntax CLI fixtures then passed 119 ordinary checks (full, lazy,
lazy+verification with both stack policies) and 63 ROS checks (full with both
policies), including Wasmtime 48.0.2 and explicit per-feature disables. WABT
1.0.41 asserts on named atomic memargs; these fixtures use equivalent numeric
indices. Default legacy validation rejects FE as an illegal opcode, whereas an
explicit scoped disable gives the precise enable-threads diagnostic.
Host-provider atomic access remains unavailable: a non-atomic provider read must
never be silently substituted for an atomic read. Native imported aliases work.
Evidence: `/work/artifacts/atomic-loads-r{27,28,29}` and local
`build/wasm3-evidence/atomic-loads-r{27,28,29}`.

### Atomic store qualification and RMW groundwork (r31–r32)

Seven stores passed 108 binary cases (28 accepted, 80 rejected) in each of four
interpreter configurations: uncached, merged rings, separate rings and a one-slot
integer ring. Tests retain a live value across two stores, check full/narrow
zero/all-one/high-bit inputs, all neighboring bytes and the unselected memory,
wrong address/value types, missing operands, exact alignment, unreachable code,
feature disables, page-end accesses and real misalignment/u33/bounds traps.
LLVM passes both instruction and native-unwind execution/stack diagnostics.

All 172 actual interpreter tail handlers have one atomic xchg and an indirect
successor jump, with no calls or extra fences in the straight-line success path.
All 28 actual optimized LLVM modules per product retain two naturally aligned
sequentially consistent stores. Ordinary mmap page preflights fold away, and the
unwind success path contains no bookkeeping calls. Actual store-plus-load CLI
fixtures pass 119 ordinary and 63 ROS checks, including Wasmtime 48.0.2, scoped
feature disables and ordinary JIT full/lazy/lazy+verification.

The raw-memory RMW helper (not a declaration of complete guest RMW support) passes
ASan/UBSan and QEMU PPC64 big-endian/ARM32 checks in both repositories. These cover
all byte widths and seven RMW families, CAS match/mismatch with wrapped expected
values, unchanged neighbors and four concurrent fetch-add histories. Big-endian
addition/subtraction uses a native-pattern CAS loop with guest-endian arithmetic;
bitwise operations and exchange commute with byte reversal. Actual LLVM big-endian
RMW lowering still requires target qualification.
Evidence: `/work/artifacts/atomic-stores-r31` and `/work/artifacts/atomic-rmw-r32`.
Selected old generated ROS binaries from r25–r29 were archived locally as
`build/wasm3-evidence/archive-thread-binaries-r25-r29.tar.gz`; every member hash
was verified before removing only those remote generated files.

### Guest RMW execution (r33)

All 49 opcodes (seven families times seven widths) pass 756 binary cases per run:
196 accepted and 560 rejected. Both products pass uncached, merged, separate and
one-slot integer-ring interpreter configurations, plus full LLVM with instruction
and native-unwind stack reporting. Cases verify old-value results across repeated
operations, wraparound, successful/failed narrow cmpxchg including ignored high
expected bits, untouched neighbors/other memory, invalid operand types/counts,
exact alignment, disabled features and genuine alignment/bounds/u33 traps.

The 1,204 instantiated RMW tail handlers per product retain indirect tail dispatch;
their straight-line success paths have no calls or extra standalone fences. All
196 actual optimized LLVM fixture modules retain their two SC RMW/CAS operations,
natural alignment and CFI; native-unwind success paths omit instruction bookkeeping.
On ordinary 64-KiB mmap pages, redundant store preflights fold away. Unusual custom
page paths still reuse the existing conservative store preparation policy.

Two batched WAT modules execute all 49 mnemonics plus separate CAS mismatch cases
(112 cases per configuration). Ordinary int/JIT full, lazy and lazy+verification
pass, including both JIT stack policies, Wasmtime 48.0.2 and scoped feature disables.
The ordinary int runner records 11 checks, ordinary JIT 17 and ROS full JIT 9.
Ordinary lazy and lazy+verification also passed the all/no-T0/no-T2/no-T0-no-T2
tiered configurations (9 batched checks each). These short cases verify selected
configuration paths, not natural hotness-triggered T2 promotion. This is not full threads conformance
or a complete cross-platform interpreter/JIT matrix. At r33, guest shared-memory types, wait/notify and lazy-unwind concurrent metadata
publication remained outstanding; later development is recorded below.
Evidence: `/work/artifacts/atomic-rmw-r33`.

## Verification environment

All compilation and execution use SSH host `linux`, container
`uwvm3-implementation`, rooted at
`/home/macromodel/Documents/uwvm3-implementation`. No compilation or tests were
performed on the local Mac. The aggregate cgroup has:

- `memory.max = 68719476736` (64 GiB), `memory.swap.max = 0`.
- `cpuset.cpus.effective = 0,2,4,6,16-31`.
- CPUs 16–31: sixteen E-cores. CPUs 0,2,4,6: one logical CPU from four P-cores.
- Dropped container capabilities and `no-new-privileges`; all test child processes
  and compilation jobs inherit the cgroup.

Ordinary native tests use Clang/LLVM 23 revision
`4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89`. ROS native tests use its bundled
**23.1.1-uwvm-ros.7**, built from its verified source manifest and CMake contract.
Earlier `.6` results retain their original toolchain identity.
The ROS LLVM build was also expanded to X86, AArch64, ARM, RISCV, PowerPC, SystemZ,
Mips, Sparc, and LoongArch for offline generated-code checks.

WABT 1.0.41 assembles the fixtures. Wasmtime 48.0.2 is the independent reference.
Official WAST inputs are pinned by SHA-256 in the corresponding runners.

## Completed focused checks

- Parser/initializer ASan and UBSan checks in both repositories, including invalid
  and truncated expressions, feature policy, prior/imported globals, preserved NaN
  bits, table fill order, imported reference identity, and segment offsets.
- 22 selected official `global.wast` cases against Wasmtime. This includes the
  module at line 634 with explicit table initialization; it is a parser comparison,
  not execution of the entire Core suite.
- Extended-constant/table execution in uncached and register-ring interpreters,
  including local.get/add/local.tee fusion checks across four combine settings and
  three delay settings.
- Relaxed SIMD: 25 independent byte-vector cases and 126 validation cases in each
  of the same 12 interpreter configurations, plus ASan/UBSan. Both uncached and
  register-ring dispatch paths run. Validation covers unknown opcodes, operand
  underflow/types, overflowing LEBs, polymorphic unreachable code, and disabled
  features.
- Every assertion in seven pinned official Relaxed SIMD WAST files: 69 assertions
  in eight modules. The wrapper retains full `either` result sets and NaN patterns.
  Ordinary runs use 17 backend configurations (full/lazy/lazy+verification int/JIT,
  both JIT stack policies, and tiered variants), plus Wasmtime: 161 module runs
  including the UWVM fixed-projection fixture. ROS uses int full and JIT full with
  both stack policies: 27 module runs. The truncation WAST has no assertions;
  conversion execution is covered by the independent byte-vector fixture.
- The fixed-projection fixture is not used as a bit-equality oracle for another
  engine: multiple results are permitted for relaxed instructions.
- Actual initializer CLI execution and trap stacks were checked in ordinary full,
  lazy, lazy+verification and tiered configurations, plus ROS full backends. Both
  instruction and unwind produce `[0, 1, 2]` at the deliberate nested trap; the
  disabled stack policy produces no frames. The current native JIT binaries also
  passed the initializer regression after Relaxed SIMD was added.
- Multi-memory: 60 new binary fixtures (48 valid and 12 invalid), including all
  scalar memory widths, SIMD memargs/lanes, bulk operations, selected growth limits,
  imported aliases, padded indices, memory index 129, and malformed encodings.
  Both standalone and integrated validators run with enabled/disabled policies.
  All 12 combine/delay configurations pass in both repositories, with uncached
  and register-ring execution; both also pass ASan/UBSan. The immediate scanner
  has a separate sanitized truncation/overflow/transaction test.
- Actual multi-memory CLI fixtures pass 18 ordinary configurations including the
  standalone validator (1080 runs) and four ROS configurations (240 runs). Wasmtime
  passes 58 fixtures. Its hard validator limit of 100 memories prevents comparing
  the two index-129 fixtures; these are recorded as reference limits, not passes.
- The pinned current Wasmtime multi-memory `simple.wast` runs its original named
  and imported-memory syntax: seven modules, 17 assertions, in all 17 ordinary
  runtime configurations plus Wasmtime and all three ROS runtime configurations.
  Additional named-import alias/linking fixtures pass each configuration. WABT
  enables multi-memory specifically, avoiding post-Core-3 compact-import syntax.
- Eight new indexed-memory bounds traps execute through three nested Wasm calls.
  Instruction and unwind retain exactly `[0, 1, 2]`; `none` retains no frames.
  Ordinary int/JIT/tiered variants and ROS full backends pass, with Wasmtime as an
  independent bounds-trap reference (168 ordinary/reference and 32 ROS runs).
- Native LLVM provider tests pass 18 transfers and 24 generated-code bounds traps
  per repository, including aliased forward/backward overlap beyond 4096 bytes,
  mixed provider/native memories and zero-length boundary cases. Actual translator
  IR references distinct objects; scalar/SIMD leaves contain two instruction-frame
  calls or zero unwind-frame calls. Cross-memory copies add one operation bridge.
  O3 assembly has direct memory references and unwind CFI. Ten actual interpreter
  memory handlers per repository preserve indirect tail dispatch; checked guarded
  scalar load/store paths add no helper calls. Cache tests now include actual
  multi-memory object-cache hits and independent feature rejection (10 cases).
- Native interpreter assembly: the six new binary/ternary ring handlers in each
  repository contain no helper calls and end in indirect tail dispatch. Actual
  LLVM translator IR has two frame-record calls per tested leaf in instruction
  mode and zero in unwind mode. The optimized unwind madd body contains native
  multiply/add and return with unwind metadata.
- Cache ABI/product isolation: the current v15 fixture passes 36 signed/unsigned
  invocations, including rejection of the previous v12 table layout and v13/v14
  memory layouts. The cached-feature CLI runner additionally
  passed seven cases per repository with verified source provenance, including
  actual cache hits followed by independent rejection of each disabled feature.

The cross runner compiles the **production SIMD lowering** and an independent C
oracle using Clang/LLVM, then runs 25 vectors under QEMU 11.1.1. Both repository
emitters passed all 19 profiles: i386 SSE2; AArch64 NEON/scalar; RISC-V scalar/vector;
ARM hard-float NEON/scalar and soft-float; PPC64LE/PPC64 VSX and PPC32 scalar;
SystemZ vector; MIPS/MIPSEL 32/64; SPARC64 scalar; LoongArch LSX/scalar.

The complete 19-profile run uses the built ROS LLVM for both emitters because the
installed ordinary LLVM omits PowerPC/SystemZ/SPARC targets. Thirteen corresponding
profiles also passed using the installed ordinary LLVM. Clang emits C-oracle IR;
the matching LLVM `llc` emits both objects. Debian PPC and SPARC CRTs use their
matching GNU linker. Debian MIPS CRT executable-stack metadata is honored only in
these fixed-data C test programs, without changing VM build flags.

These are **generated-object tests**. They do not establish complete cross-target
CLI/JIT execution, unwind behavior, EH, or all-platform acceptance. The target CPU
must be supplied when inspecting actual native IR (`raptorlake` on this host),
since its SIMD intrinsics reflect the native JIT's target features.

A separate cross runner now compiles the actual parser, initializer, standalone
and integrated validators, and both interpreter dispatch paths for the 60 new
multi-memory fixtures. Its baseline exposed 32-bit reservation exhaustion,
i386 test callback calling-convention mismatch, a missing MIPS test codegen flag,
and a SystemZ LLVM load/trap sinking bug. The r4 ordinary rerun passes all nine
selected i386/ARM/SystemZ/MIPS profiles. The complete ROS r4 run passes 16 of 19
profiles; three older PowerPC profiles still fail the forced-ring `musttail`
eligibility check. They remain recorded failures and use uncached production
execution. A separate POWER10 ELFv2 PC-relative profile passes both fixture
interpreters, and the ordinary actual CLI passes full/lazy/lazy+verification plus
standalone validation. Its nine inspected scalar/bulk memory ring handlers end
in `bctr`; scalar handlers have no helper calls. Only that narrow ABI/CPU/compiler
combination opts into the two-register integer/floating ring. The ROS actual
POWER10 CLI also passes full interpreter execution and standalone validation,
including the eight nested bounds traps and all nine assembly checks. No `musttail` annotation or machine verifier
has been removed to obtain a pass.

The SystemZ repair is retained in ROS LLVM revision `.7` and supplied to ordinary
uwvm2 as `documents/toolchain/patches/llvm-systemz-load-trap-motion.patch`.
It forbids unsafe load sinking, fixes base/index liveness, and preserves memory
operands. Reduced real interpreter IR passes machine verification and 999 QEMU
alias/cursor executions; adjacent load/trap fusion remains present. Exact `.7`
header guards pass all eight controls. The native `.7` package and both actual
r4 CLIs were rebuilt; the complete native new-syntax, provider, IR, matrix and
cache suites pass on their recorded source fingerprints. Earlier `.6` results
are not relabeled.

## VM execution ownership (r25–r26)

`utils/thread/execution_domain` adds reusable generations to the generic lifetime
primitive. Closing rejects new admissions and requests cooperative cancellation;
reset drains the old generation, runs resource cleanup, then publishes a fresh
cancellation source. Cleanup failures leave admission closed. Neither stop nor
reset forcibly kills guest computation; a non-returning execution can delay reset.

Both runtime/lib implementations now register their outermost full/lazy/public-raw
entries (ROS full/raw only). The lease outlives TLS/map cleanup and native stack
restoration. Supported nested raw callback reentry uses the outer admission, even
while another host is resetting. Reset from the executing thread still fails
closed. Reset and global teardown drain admissions before destroying JIT engines,
module registries and borrowed function/address tables. Host-owned OS threads and
external module loader/configuration changes still require host coordination.

Full JIT no longer holds the mutable-unwind execution mutex across an invocation:
its code and unwind maps are immutable after full publication, and the new domain
holds them alive until all executions return. This allows two host threads to
enter generated functions and rendezvous in callbacks in native-unwind mode.
Ordinary lazy/tiered unwind now publishes entry/range snapshots and does not hold
a mutex across guest execution (r35–r36 below). This does not switch unwind back to
instruction bookkeeping. Blocking Wasm wait support still needs shared-memory and
instruction integration.

Both products passed the r25 focused ASan/UBSan domain test with NDEBUG: bounded
admission, cancellation, drain-before-cleanup, distinct generations and fail-closed
exception recovery. Both passed the r26 actual full interpreter entry test and
full/raw LLVM tests in explicit instruction and checked unwind modes, including
concurrent reset during a live import, callback reentry after cancellation,
`atomic.fence` with live values, fresh-generation execution and a two-thread
barrier. Evidence: `/work/artifacts/execution-r25` and `execution-r26`. These are
narrow development tests, not a complete thread-safety or platform qualification.

## VM-owned lazy workers and immutable unwind publication (r34–r36)

The Wasm-independent `utils/thread/immutable_snapshot` utility registers readers
outside signal paths, publishes an immutable payload using lock-free pointer
atomics, and retires versions only when no reader holds them. Both repositories
passed retained-reader, replacement, clear, exception rollback and concurrent
publication tests under ASan/UBSan and TSan. Docker ASLR layout prevented some TSan
startup attempts; these environmental failures are retained in the evidence. A
non-PIE test executable was used without changing cgroup/seccomp/ASLR policy.

Ordinary lazy unwind now copies writer-owned address entries and code ranges into
one snapshot. Outermost host execution registers a reader; nested raw callbacks
reuse it. Trap lookup copies the resolved entry before releasing the hazard. Full
mode retains direct lookup of frozen vectors. No generated guest memory operation
or function entry acquires a lease, takes a snapshot lock, or records a logical
native-unwind frame. Background unwind compilation stays disabled pending its
separate direct-call graph grouping audit; concurrent host demand materialization
is supported by the new publication protocol.

Lazy execution no longer stops global compiler schedulers at each return. A VM
generation owns them; reset drains host entries, joins workers, then clears their
borrowed registries. A first-entry process-lifetime guard additionally drains and
joins before namespace-static caches/thread maps are destroyed. Deferred scheduler
readiness uses acquire/release publication instead of reading a mutable worker
count. Process-wide timing records are serialized at host boundaries. Explicit
proc-exit shutdown is terminal; reusable concurrent administration uses stop/reset.

Focused r36 actual ordinary int/JIT tests passed full and lazy entry, instruction
and native unwind, two-thread import rendezvous, four concurrent cold-unit callers,
two configured compilation workers, reset during a live callback, callback raw
reentry after cancellation, and fresh-generation execution. Native-unwind policy
keeps background workers disabled; its publication stress comes from host demand
compilation. Actual lazy trap diagnostics preserve function 36 and caller 37 in
both stack policies. ROS full int/JIT tests passed the same lifetime/cold-caller
fixture and both JIT stack policies. Generic scheduler readiness polling passed
TSan. r38 ordinary lazy LLVM runtime also passed TSan with instruction and checked
unwind policies after the fixes below. The dual-backend lifetime fixture passed
all/no-T0/no-T2/no-T0-no-T2 under both policies. These short calls do not prove
hotness-triggered T2 promotion. ROS r38 full JIT also passed both stack policies.
The byte-identical generic snapshot header/test passed Clang cross compilation,
LLVM machine verification, and QEMU execution on ARM32 and big-endian PPC64;
these two utility probes do not qualify target JIT unwinding.

TSan first exposed a Linux signal-stack cache teardown problem: repeatedly rearming
a resource-free pthread destructor runs it after sanitizer thread-state retirement.
Linux now relies on its persistent trivial TLS retirement flag; Darwin retains its
TSD marker for TLV re-creation. Both products passed the existing native stack guard
suite (including late destructor reentry) and a new TSan thread-exit regression.
TSan then found concurrent writes to the process mmap fault callback at each VM
entry. Both runtimes now initialize that stable callback once before any caller
executes. The ordinary lazy snapshot reader uses OS TLS independently of optional
VM TLS caches, avoiding a thread-map lock during fault-time lookup.

The r37 native-entry microbenchmark alternated five old/new runs on CPU 0. Median
CPU ns/entry were 340.30 -> 341.68 (main), 247.69 -> 247.17 (pthread), 171.52 ->
171.87 (host alternate stack), and 1.44 -> 1.44 (nested). This narrow measurement
shows no large change; it is not a whole-VM throughput or statistical equivalence
claim. All 166 compiler/memory files matched the r33 source byte-for-byte, preserving
the scope of that actual atomic IR/assembly qualification.

Evidence: `/work/artifacts/snapshot-r34`, `lifecycle-r35` through `lifecycle-r38`.
These results do not complete shared-memory/wait/notify semantics or the whole
Core 3.0/debugger/hot-replacement task.

## Native unwind capability repair and actual target execution

`native_unwind_platform.h` now selects the registration ABI rather than an x86-64
CPU allow-list. ELF/DWARF and Mach-O builds with the unwind interface can attempt
the real generated recursive-chain probe. Win64 retains its SEH backend. ARM32
EHABI and SJLJ do not have the required dynamic registration implemented here;
they are not advertised as DWARF backends. Explicit ARM/Thumb
`__ARM_DWARF_EH__` builds remain eligible and configure the LLVM TargetMachine
to emit DWARF CFI. Its native TargetMachine policy test passes; this is not an
ARM32 full-JIT execution qualification. The CLI, help, version information and
runtime use the same capability header. Explicit checked unwind fails if the
probe fails; only automatic policy may select instruction mode on a failed probe.
Successful unwind mode emits no JIT logical-frame push/pop calls.

The r5 Linux AArch64 full VMs link and execute their actual target LLVM `.7`
library under QEMU. Both ROS runtime modes and both JIT stack policies pass all
60 new multi-memory fixtures and eight nested traps. Ordinary full/lazy/lazy with
verification and tiered policies, including native unwind combinations, pass
the expanded 26-configuration suite (1560 fixtures and 200 nested traps).
These are actual CLI/JIT runs,
not cross-generated object-only tests.

`run_wasm3_native_unwind.py` adds indexed scalar/SIMD memories, extended-constant
initialization and relaxed SIMD to eight-level recursive trap tests. Both native
x86-64 products pass 72 runs each: instruction/unwind/auto, debug/O3, cold execution
and two authenticated object-cache replays. Both AArch64 products pass the 72 fresh-code
runs. AArch64's existing process-local address lowering prevents reliable cache
hits for these modules, so its cache replay attempt is retained as a failure;
`--cache disabled` explicitly measures fresh generated code instead. The generated
policy must be `call_stack=unwind`, `unwind_check=live`, and `call_stack_frames=omit`
for both requested unwind and auto. An instruction fallback fails this test.

`check_wasm3_native_frame_codegen.py` inspects objects emitted by the running
full-JIT CLI for an actual new indexed-memory leaf. Both products on x86-64 and
AArch64 pass: instruction has exactly two frame-maintenance calls, unwind has
zero, and native unwind retains `.eh_frame`. This inspects the actual emitted
machine code independently from the runtime policy logs.

The first actual RISC-V CLI runs exposed a missing LLVM assembler parser: the
production full-width `li` address materializer aborted during code generation.
Full and lazy initializers now register RISCVAsmParser, and ROS's CMake contract
and ordinary LLVM component discovery require its library. The repaired r6 ROS
CLI passes 240 fixtures and 24 nested traps; ordinary r6b passes 1560 fixtures
and 200 traps. Both pass 72 fresh-code native-unwind runs. RISC-V intentionally
disables its process-local-address object cache, so these are not cache-hit
results. Actual target-emitter IR also passes eight O3 code-generation checks.

The PC-range map and generated-chain probe now distinguish callable addresses
from instruction addresses. PPC64 ELFv1 function descriptors remain callable
with their TOC; only their first code-address word enters PC comparisons.
ARM/Thumb clears the state bit only for PC comparison. Native unwind cursor
tests pass on 13 QEMU ABI profiles, including PPC64 ELFv1 descriptors. These
native ABI tests do not substitute for a full target LLVM JIT qualification.
The i386 target LLVM library is built, but its interrupted CLI builds are not
accepted results. PPC full-JIT qualification also remains pending.

## Memory performance checks in progress

The mmap hardware reservation remains enabled. Its empty operation guard adds
no allocation pin or lock to the scalar access path. Necessary store-boundary
preflights remain: a split store must not modify an in-bounds prefix before an
out-of-bounds trap, as required by the Core memory-store execution rules.

New native benchmarks compare repository HEAD and working-tree execution of
aligned scalar, cross-page unaligned scalar and SIMD loops. Each has legacy
memory-0 and Core 3 indexed-memory variants. Compilation/initialization is
outside timing, results are checked, runs are interleaved on allocated P-core 0,
and binary/fixture hashes and paired confidence intervals are retained.
Ordinary full JIT with PassBuilder O3 passes the initial instruction and unwind
comparisons; ROS also passes both policies with a nonzero checksum. The ROS
isolated HEAD baseline updates only its `.6` version assertion to `.7`, allowing
both versions to use the identical verified bundled LLVM build. This overlay
and its before/after hashes are recorded; no production version check is relaxed.
This focused timing result is not an absolute performance guarantee.

The interpreter benchmark found a roughly 20% scalar difference when a second
memory changed allocation layout. A two-memory module still accessing memory 0
reproduces it; both syntaxes emit identical handler sequences and code sizes.
Recorded streams show a hot branch-target pointer crossing a cache line only
for the slower layout. A 64-byte bytecode allocation alignment is being tested
in both products. SIMD memory handlers now select the immutable mmap/software
policy during compilation, as scalar handlers already do; full mmap loads no
longer inspect the policy per access. Store-boundary preflights remain present.
Both current r11 ASan/UBSan multi-memory suites pass on the allocator backend
(the existing build policy disables mmap under ASan). Hardware mmap is checked
separately through native/QEMU execution, actual fault callbacks and UBSan. The final native x86-64
assembly check covers 76 full-mmap ring handlers per repository: no helper calls,
locks or CPU memory fences, branch-free loads and indirect tail dispatch. Every
matching scalar specialization is instruction-identical to HEAD, including its
hot metadata offsets. Current v15 cache isolation also passes all 36 invocations.

Performance acceptance remains **open**. Generic r11 timings still regress for
unaligned scalar loops; production-like `-march=native` r12 reduces the ordinary
unaligned difference to about 5%, but it still exceeds the recorded confidence
bound. SIMD improves in both profiles. The r8–r12 failed/noisy comparisons remain
available; no failed result is replaced with a passing rerun. A same-process
relocation diagnostic executes the exact same handlers with different byte-stream
addresses and reproduces large scalar layout sensitivity. Its synthetic relocated
streams are diagnostic evidence, not production acceptance. The r13 fixed
function-alignment experiment regressed native scalar timing by 16–26% and was
rejected. No production function-alignment flag was added. The r11 complete
12-configuration combine/delay functional matrix passes in both products with
unchanged before/after source fingerprints. Both
native interpreters also pass 102 split-store prefix checks each: scalar, SIMD
and lane stores trap before changing either selected or unselected committed
memory, checked inside the actual fault callback.

Actual full LLVM JIT r11 also passes 51 split-store prefixes per stack policy
(instruction and unwind), in each product. A child-process signal observer checks
the selected mmap guard address and all committed bytes before exiting; this is
separate from the recursive unwind/backtrace tests. It does not alter generated
code. The r11 mmap allocation test now uses the production vector, relocates 130
records, and verifies alignment, growth and fault ownership. Native UBSan passes
in both products; i386, ARM hard-float and AArch64 QEMU pass in both products.

The next optimization selects a fixed 64 KiB store preflight at translation time
only for full wasm32 mmap reservations with that immutable page size. The last-byte
fault probe and compiler ordering fence remain; loads reuse their existing
specializations. Other pages, partial reservations, allocator-backed storage and
non-tail scalar execution keep their existing checked paths. This policy also
covers ordinary/heavy fused stores and SIMD/lane stores. The initial r14 native
assembly has 18 scalar store specializations reduced from 26 instructions to
17–18, with no calls, locks or CPU fence; 102 ROS split-store prefix checks pass.
The r15 refinement avoids generating duplicate load handlers. Its complete
12-configuration multi-memory functional matrix passes in both products. The
none/none configuration also passes 204 split-store prefix checks and 102 positive
old-boundary checks per product: the same compiled functions execute before and
after memory.grow, observe the old boundary becoming writable, and trap at the
new boundary without modifying a committed prefix. Assembly covers 103 native
full-mmap ring handlers per product, including 18 shortened scalar stores.
Before/after production fingerprints are unchanged in each completed run:

- Ordinary: `78694d8c1cc4eccf628758e4cb97cf375e408b4866a5226c9924eb649fd52827`.
- ROS: `b63404342a59d3c94b09ece33c78ebbb555a70abc5e47eb6c9f4bf5c37e95ca7`.

The r16 harness adds independent read regions initialized with a deterministic
byte pattern. All twelve fixtures (six kernels, each with legacy/indexed syntax)
pass 24 independent Wasmtime executions with nonzero checked results. Actual full
JIT objects retain the memory reads in both the original and new fixtures; the
new cases additionally prevent a same-address store from supplying the read's
value. Both interpreters pass all twelve native none/none comparisons in the
21-round quiet run, using the default 5% paired upper-confidence gate. The complete
combine/delay performance matrix is a separate, still pending acceptance item.

Both products also pass 36 actual full-JIT object inspections each. All compared
HEAD/current and legacy/indexed leaf instruction streams are identical after
normalizing symbol names. Instruction mode has two frame-maintenance calls;
unwind mode has zero and retains CFI. No CPU fence or lock enters these loops.
This code inspection does not by itself establish wall-clock performance.

The r16 JIT independent-read timing groups did not all pass. A predeclared r17
experiment increases every sample to 80,000,003 iterations and 61 interleaved
rounds, preserving all earlier failed evidence. Ordinary unwind and both ROS
policies pass; ordinary instruction has one inconclusive indexed/legacy aligned
read comparison (ratio 1.0258, 95% interval 0.9968–1.0550). Its medians are almost
identical, but its raw distribution contains slower samples. Thread CPU time and
per-thread scheduling/fault diagnostics are being added to the measured interval
to establish the cause; no threshold is relaxed and no sample is discarded.

Timings exclude translation, check every result, and retain raw samples, fixture
hashes, source fingerprints and compiler commands. The 5% paired-confidence
threshold is a regression detector, not a universal maximum-performance promise.
Current store-policy cross-platform checks and all configuration performance
checks remain required; previous r11 results are not attributed to changed source.

The completed ROS r11 split-store QEMU matrix passed both dispatch paths on 17 of
20 profiles, retaining the three legacy PowerPC forced-ring musttail failures as
failures. POWER10 ELFv2 PC-relative ring dispatch passed. Current-source growth
checks and separate legacy-PowerPC uncached execution are still pending.

A separately extracted Debian Clang 23.1.1-2 host compiler supplies the previously
missing PowerPC/SystemZ/SPARC code-generation backends. Its five C++26 pack-indexing
compile/link/QEMU smoke profiles pass (PPC64LE/BE, PPC32, SystemZ, SPARC64), with
signed repository package metadata and SHA-256 manifests retained. This host
compiler does not replace ROS's bundled target LLVM 23.1.1-uwvm-ros.7 library or
its SystemZ patch. Complete target-JIT qualification still uses that library.

The remote filesystem also filled during the first r5 matrix/build attempts.
Those interrupted runs are not passes. Completed task-owned artifacts were
losslessly compressed or archived locally with per-file SHA-256 verification;
source files and other workloads were not removed. All compilation and execution
continue within the unchanged remote cgroup. The local archive only stores bytes.

The r18 focused diagnostic rerun of the remaining ordinary-JIT aligned-read
comparison passed: current/HEAD ratio 1.003746 (95% CI 0.9802583–1.0291150),
indexed/legacy ratio 1.014469 (0.9892437–1.0390649), with 61 rounds of 80,000,003
iterations. Guest-region CPU-time diagnostics tracked wall time, including slow
samples, with no faults and mostly no context switches. These observations do not
establish scheduler noise as the cause of previous inconclusive runs; r16/r17
results are retained. This evidence is for r15 production, before the new cold
owner diagnostic field and threads slice. The r18 full interpreter performance
matrix binaries and r19 native store matrix completed building/running respectively;
new full performance timing is deferred to final qualification.

## Reproducible focused runners

- `tools/ci/run_wasm_execution_domain.sh OUTPUT int|jit` (ordinary lazy: `UWVM_TEST_EXECUTION_LAZY=1`)
- `tools/ci/run_immutable_snapshot.sh OUTPUT`
- `tools/ci/run_immutable_snapshot_cross.py REPOSITORY OUTPUT`
- `test/0017.runtime/native_stack_thread_exit.cc` (TSan regression)
- `tools/ci/run_wasm3_extended_const.sh OUTPUT sanitizers`
- `tools/ci/run_wasm3_extended_const_int_matrix.sh OUTPUT`
- `tools/ci/run_wasm3_relaxed_simd.sh OUTPUT sanitizers|matrix`
- `tools/ci/run_wasm3_multi_memory.sh OUTPUT sanitizers|matrix`
- `tools/ci/run_wasm3_multi_memory_llvm.sh OUTPUT FIXTURES`
- `tools/ci/build_wasm3_llvm_cli.sh OUTPUT`
- `tools/ci/build_wasm3_cross_llvm_cli.sh OUTPUT`
- `tools/ci/build_wasm3_cross_int_cli.py OUTPUT ...`
- `test/0011.initializer/run_wasm3_const_spec.py`
- `test/0014.llvm_jit/run_wasm3_initializers.py`
- `test/0014.llvm_jit/run_wasm3_multi_memory.py`
- `test/0014.llvm_jit/run_wasm3_multi_memory_spec.py`
- `test/0014.llvm_jit/run_wasm3_multi_memory_traps.py`
- `test/0014.llvm_jit/run_wasm3_qemu_cli.py`
- `test/0014.llvm_jit/run_wasm3_native_unwind.py`
- `test/0014.llvm_jit/check_wasm3_native_frame_codegen.py`
- `test/0013.uwvm_int/wasm3/check_power10_memory_assembly.py`
- `test/0014.llvm_jit/check_wasm3_multi_memory_ir.py`
- `test/0013.uwvm_int/wasm3/check_multi_memory_assembly.py`
- `test/0014.llvm_jit/run_wasm3_relaxed_simd.py`
- `test/0014.llvm_jit/run_wasm3_relaxed_cross.py`
- `test/0014.llvm_jit/run_wasm3_int_cross.py`
- `test/0014.llvm_jit/run_llvm23_systemz_load_trap.py`
- `test/0014.llvm_jit/run_wasm3_cached_features.py`
- `test/0014.llvm_jit/check_wasm3_relaxed_ir.py`
- `test/0013.uwvm_int/wasm3/check_relaxed_assembly.py`

The shell entry points reject a missing cgroup or different memory/CPU limits.
Python runners inherit this same externally enforced sandbox. The CLI builder
fingerprints the actual source archive before and after compilation; a changed
source digest invalidates the binary. It never fabricates a Git commit or enables
unsafe cache provenance exceptions.

## Local evidence copy

`build/wasm3-evidence/` contains the focused result JSON, sanitizer/matrix logs,
IR and assembly extracted from the remote run. It is ignored build output; the
runners above are the reproducible source. `wasm3-focused-verification.json`
explicitly records `overall_task_complete: false`, source digests, cgroup limits,
and the remaining scope. A later cross-C++ run hit the 64 GiB cgroup limit:
`memory.peak=68719476736`, `oom=1`, `oom_kill=1`. That killed build is a failure,
not a passing result. Cross-C++ runners now default to one job and cap their own
parallelism at two; all concurrent jobs still share the unchanged aggregate
64 GiB cap. Completed IR/assembly is losslessly compressed to preserve evidence
within the remote disk space.

## Shared memory and managed guest waits (r39–r64 development)

Shared declarations now decode flag 3 with a mandatory maximum; flag 2, unknown
flags, invalid/truncated u32 limits and missing feature authorization fail.
The initializer independently enforces sharedness, exact shared/unshared import
matching and a concurrent native-memory backend. Host provider memories lack that
shared ABI and are rejected. Both r48 parser suites passed 1,060 binary cases,
metadata preservation, independent initializer gates and four import combinations.
A pre-existing import-details formatter progress bug exposed by these cases was
fixed without modifying the old pure validators.

The Wasm layer is `runtime/wasm_threads`; Wasm-independent keyed queues,
cancellation and generation ownership remain under `utils/thread`. VM host entry
binds a domain and cancellation token. Wait32/64 share native owner+offset identity,
retain no movable allocation address while blocked and release allocator pins
before sleeping. Wait/notify use explicit bounds even on mmap so a native fault
cannot escape with a shard mutex held. Ordinary memory instructions do not use
this helper. Generic cancellation never becomes a fourth guest return code.

Shared size/grow use SC publications; allocator readers use a dedicated atomic
length, while mmap keeps policy metadata in existing padding. r46 TSan and UBSan
checks passed concurrent growth and move/publication cases. r49 ARM32 and big-endian
PPC64 QEMU tests passed both shared-size and wait-helper suites. These are native
helper tests, not full guest JIT qualification on those targets.

At r48 the actual x86 interpreter object's 528 ordinary memory, 735 atomic and
3 size handlers had instruction-identical successful paths against r36 and
retained indirect tail dispatch. At r47/r49 both products' generated size/load
IR and optimized x86 machine code passed shared/unshared comparisons; unwind
success paths introduced no instrumentation calls or fences. This is focused
codegen evidence, not the full benchmark or combine/delay acceptance matrix.

Guest FE00/01/02 validation and both emitters are now implemented, including the
always-i64 wait timeout and exact alignment. r51 full interpreter VM execution
passed real concurrent wait32/wait64/notify with older mixed ring values. r54
interpreter by-reference, merged-ring and separate-ring checks each passed 15
accepted and 41 rejected cases, including indexed memory, zero-count notify
bounds, u33 overflow and non-shared wait traps.

The initial JIT concurrency test failed: current Clang prints the same
`__PRETTY_FUNCTION__` identity for distinct numeric function-template
specializations. Same-signature wait bridges consequently overwrote each other
in MCJIT's global symbol table. r55 adds stable FE-opcode discriminators to ALL
atomic bridges, including allocator load/store/RMW fallbacks, and bumps the
cache ABI to v19. r55 ordinary/ROS full JIT and r56 ordinary lazy JIT subsequently passed actual
concurrent guest waits under both stack policies. Both products' JIT validation
and trap matrices passed 15 accepted/41 rejected cases per policy. r56 mmap and
r57 concurrent-allocator tests each passed 66 coexisting atomic operations under
both policies, preventing single-opcode modules from hiding bridge collisions.
The earlier failure remains in the evidence. r59 ordinary lazy interpreter, ROS
full interpreter and ordinary allocator full JIT also passed the updated fixture
with a grow interleaved between guest notifications.

Host cancellation of a guest wait currently enters the existing fatal trap
reporter after releasing locks; it does not implement recoverable guest unwinding.
The earlier reset test cancels a host callback's structured helper wait and drains
its execution lease. Do not interpret that test as successful recovery of an
arbitrarily suspended guest activation. r58 initialization now pins moving native memories through the complete active-segment
copy and applies the shared data-section synchronization fence. Both ASan/UBSan
initializer suites passed local/imported shared/unshared overlapping segments and
an allocator grow forced to contend with the initialization snapshot.
Full official threads conformance, complete execution-mode coverage, concurrent
module loading and full platform tests remain outstanding.

r60 ordinary tiered all/no-T0/no-T2/no-T0-no-T2 configurations each passed the
updated VM wait/notify/grow fixture with instruction and unwind policies (eight
runs). ROS concurrent-allocator full JIT passed both policies and all 66 coexisting
atomic bridge operations. These short tiered cases do not prove hotness-triggered
T2 promotion. The new two-module fixture passed ordinary tiered and ROS allocator
full JIT under both policies: provider wait32, consumer wait64 via a duplicate
memory import, notification via the other alias, independent owner/offset
isolation, and shared growth while another guest is suspended.

r59 ARM32 QEMU passed actual guest wait/notify syntax, validation and execution in
uncached, merged-ring and separate-ring configurations. r59 legacy PPC64 big-endian
forced rings fail Clang's musttail eligibility check. r60 POWER10 PC-relative
compilation reaches LLVM but its extra separate i64 ring puts arguments on the
stack and fails musttail lowering in an existing i32.eqz handler. Earlier smaller
POWER10 rings remain separately qualified; neither new failure is a passing result.
The Clang source diagnoses legacy indirect calls in `clang/lib/CodeGen/CGCall.cpp`;
LLVM's `PowerPC/PPCISelLowering.cpp` rejects modified stack-passed sibling arguments.

r61 exposed an atomic C++ bridge ABI defect: Clang requires zeroext/signext
for narrow PPC64/SystemZ/SPARC64 parameters and signext for RV64/MIPS64/LoongArch64,
while the handwritten LLVM call did not express those contracts. r62 uses full
GPR carriers on these target ABIs and preserves the original i32 carriers elsewhere.
Guest i64 values retain 64 bits; the emitter truncates results to the Wasm width.
Direct mmap atomics are unchanged. All bridges use stable
`threads-fe-v2-native-scalars` discriminators and cache ABI v20.

The first universal widening attempt added constant-materialization instructions
in three x86 cmpxchg fixtures. It was rejected by codegen review. r62 instead passes
132 actual optimized native caller comparisons per product (66 operations times
two stack policies): every instruction sequence matches the qualified baseline
after symbol-name normalization. Unwind has one semantic call and instruction mode
has three, with no new fences. Both products' r62 allocator VM wait/grow fixtures
and high-bit 66-operation mixed atomic modules pass instruction/unwind execution.
Both products then passed actual RISC-V full JIT execution under QEMU with the
bundled LLVM 23.1.1-uwvm-ros.7: 66 coexisting high-bit atomic bridges, 15 accepted
and 41 rejected wait/notify programs, and VM wait/grow/cancellation/lifetime cases,
each with instruction and unwind policies. Other target ABIs still require
corresponding qualification. These code comparisons are not timing
benchmarks or the complete performance acceptance matrix.

The Linux host rebooted before the r62 continuation. The existing sandbox was
restarted and its 64-GiB / 16-E-core + 4-P-core limits were rechecked. A later
cross-test entry found `memory.swap.max=max` despite Docker's saved no-swap setting
and refused to compile. Reapplying Docker resource settings restored it to zero;
the new cross runner records and verifies cgroup controls before and after testing.
Earlier r60 PPC64-loop and two LLVM fixture builds failed during disk exhaustion;
these are not passes. SHA-verified local archives retain those artifacts and older
generated binaries/core dumps; only task-owned archived files were removed remotely.

r62 also passed 16 newly assembled wait32/wait64/notify CLI programs against
Wasmtime 48.0.2: 137 ordinary and 73 ROS checks, including feature disables, start
sections, high-bit expected values, finite/zero/negative mismatch timeouts, mixed
live operands, end-of-memory accesses and specific alignment/bounds/sharedness
traps. Ordinary full/lazy/lazy+verification and both JIT stack policies run; ROS
runs full with both policies. Wasmtime requires explicit shared-memory enabling.
Three SHA-pinned official Wasmtime regression files (commit
`3f3f222b77a198db939863d8769af6092ee547e6`) additionally passed all 17 assertions:
119 ordinary and 51 ROS executions including the reference. The wrapper appends
assertions without rewriting the original guest functions. These are targeted
regressions, not the whole official threads suite.

r63 adds the host `runtime_stop_and_drain_host_api()` in both products. Generic
`utils/thread/execution_domain::stop_and_drain(callback)` retains administrative
ownership through dependent producer shutdown, preventing reset from reopening
admission during that shutdown. The VM drains admitted executions, joins its
compiler workers and flushes accepted cache writes while retaining code/registries;
only explicit reset reopens admission. It rejects invocation from an active
execution or compiler/provider callback. Native host threads remain caller-owned;
cancellation is cooperative, and guest wait cancellation still uses a fatal trap.
No guest dispatch, JIT emitter, memory handler, or memory ABI changes accompany
this administrative API. The cache writer stays idle and reusable after flushing.

The r63 generic shutdown/reset serialization test passed ASan/UBSan. Both products'
allocator full JIT passed instruction/unwind VM tests, ordinary tiered passed all
four tier configurations under both policies, and ROS full interpreter passed.
Tests hold a callback live while an external host stops/drains the VM, permit its
nested raw call while admission is closed, reject subsequent new entries, and
execute a fresh generation after reset. ROS RISC-V full JIT also passed both stack policies under QEMU; ordinary lazy
interpreter and lazy LLVM JIT passed the same stop/reset lifecycle checks.
The new 16-program CLI suite also passed 89 ordinary interpreter checks across
full/lazy/lazy+verification and 57 ROS full interpreter checks. The three wait
handlers in each actual CLI binary retain indirect musttail jumps with no return
instruction. ROS r63 also passes all 132 JIT atomic caller comparisons against
r62: instruction-identical after symbol normalization, unwind retains one semantic
call versus three in instruction mode, with no added fences. This is a codegen
check, not a wall-clock throughput measurement. Both module dependency audits and their 45 checker unit tests pass;
old wasm1p1/wasm2 validator directories still match HEAD.

The generic shutdown/reset test also passed TSan after two startup-layout failures
(the original attempt plus one retry). Those failures were ASLR/TSan shadow-layout
incompatibilities before test entry, not passing runs; all logs are retained.
The second bounded retry ran the test successfully without changing container
security settings. The r63 artifact named ordinary `jit-lazy` inherited the tiered
fixture define and is extra tiered evidence only; `jit-lazy-r2` is the correctly
configured LLVM-only lazy test.

r64 fixes a reproduced process-teardown lifetime inversion in both products.
The cache service could first initialize after the execution lifetime guard,
registering a destructor that ran before active executions were drained. The
guard now constructs that service before registering its own destructor; this
starts no writer thread and performs no IO. Normal exit therefore drains VM
executions/compiler producers before destroying the cache mutex and queue.
`cache_process_lifetime.cc` observes actual POSIX condition-variable destruction
without reading a destroyed C++ object. Both r63 products failed with the specific
ordering diagnostic (exit 86); both r64 products passed instruction and unwind,
with the expected success marker required. Their full-JIT VM lifecycle reruns
also passed. This fix changes one-time host initialization, not guest codegen.

The r59 x86 memory-handler comparison again passed all 1,266 successful paths
against r36; the three new wait/notify handlers retain indirect musttail dispatch.
Twelve actual optimized JIT wait/notify codegen cases show one semantic bridge
call in unwind mode versus three calls in instruction mode, with native CFI.
These checks do not replace the final performance or architecture matrices.

Evidence: `/work/artifacts/shared-r39` through `shared-r64`. Archived generated
binaries are SHA-256 verified under local `build/wasm3-evidence`; failed builds
and diagnostic logs remain separate from passing results.

## Tail-call development (r65–r75)

`-WFE-tail-call` / `-WFD-tail-call` (and their long forms) independently gate
`return_call` and `return_call_indirect`. Old complete-version selectors keep
this addition disabled. This is **not yet complete tail-call support** across
all requested JIT signatures, modes, platforms, or the Core 3 reference type system.

The standalone Wasm 3 validator checks the enclosing function's full result
tuple even in unreachable code, the callee arguments, both indices, the i32
selector and the function-reference table type. Its helper is explicitly
included/exported by `wasm3/impl.h` and `impl.cppm`. No changes were made to the
`wasm1p1/` or `wasm2/` validator directories. Both r73/r75 products passed 96
pure-validator cases and the same 96 integrated interpreter-validation cases,
including padded/overflowing immediates, mismatched result tuples, dead code,
and rejecting externref tables. The current flat value representation still
cannot express recursive reference subtyping.

The interpreter implements direct/indirect tail transfers through a frame-owner
loop: arguments are owned before releasing the outgoing frame, local/operand
storage is reclaimed on every transfer, and one logical stack record is replaced.
This covers different parameter lists, mixed scalars, large/small local frames,
Wasm import forwarding, native host calls and concurrent host entries. The
ordinary full/lazy and ROS full fixture passed 60,006 Wasm transfers per run in
both direct and indirect forms, with host-stack depth bounded independently of
chain length. General transfers currently stay in the interpreter in T0 mode;
per-edge promotion to JIT requires the remaining general JIT tail ABI.

Self-tail calls reset parameters/declared locals and branch to the existing
function body. r75 removes the extra interpreter branch-handler dispatch,
clears obsolete register-ring inputs, and uses fixed-width small tuple copies.
Both products passed one million and one iterations in each of byref, uncached
musttail, and mixed scalar/vector ring configurations with heavy combine/delay.
The noinline lifetime boundary applies only to transfer frames; ordinary calls
retain their original inlining opportunity.

LLVM emits self-tail calls as body backedges, including tuple results. For
non-self calls with identical scalar/vector native prototypes, it emits verified
native `musttail` followed immediately by `ret`. Instruction policy pops the
retired logical frame before the target's push; unwind policy emits no logical
frame maintenance. Ordinary lazy JIT reads the published typed target table and
has a cold materialization bridge which returns an address without calling the
target. Differing parameter prototypes, aggregate results, general indirect/
import tail targets and non-self tiered-core transfers remain unimplemented.

Evidence so far:

- r70 interpreter CLI differential checks against Wasmtime: 54 ordinary / 36 ROS,
  including dynamic table mutation/growth, null/bounds/signature traps, per-feature
  disable diagnostics and exact retired-frame traces.
- r72 actual full-JIT object checks in both products: mutual calls are native
  jumps; unwind functions contain zero call instructions and retain `.eh_frame`.
  Both full JITs passed 12 CLI/Wasmtime/gate checks, including instruction/unwind/
  none stack policies. Unit tests also verify two musttail IR edges and execute
  1,000,001 alternating function activations with unequal local-frame sizes.
- r74 ordinary full/lazy/lazy+verification exact-prototype CLI checks: 24 passed.
  Self-tail CLI checks passed all four requested tier combinations and both
  stack policies (46 checks across four programs).
- r70 vs r59 interpreter memory assembly: all 1,266 successful plain/atomic/size
  paths are instruction-identical and retain indirect musttail dispatch.
- Both r75 complete header/module dependency scans pass. All compilation and
  execution above ran over SSH Linux inside the 64 GiB cgroup with the requested
  16 E + 4 P CPU set. Further cold-target, optimized-handler and QEMU runs are
  recorded separately; none of these focused checks is final acceptance.

The selected upstream `return_call.wast` functions are pinned at spec commit
`ba9fd9f5c23e569201265d5bda6fb8dde18ad8c0` (file SHA-256
`f3473155d523116912b9bf2a96902062043e1e66c3fe195248211c4bd203096e`).
Only unchanged `fac-acc` and `count` bodies with seven assertions are selected;
the complete upstream file uses additional type-system features still missing.
Normative rules: [validation](https://webassembly.github.io/spec/core/valid/instructions.html#valid-return-call),
[execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-return-call),
and [LLVM call/musttail constraints](https://llvm.org/docs/LangRef.html#call-instruction).

## Native general tails and tiered ownership (r76–r82 development)

Both products now emit true native scalar/vector tail transfers through a private
`tailcc` typed ABI on qualified X86, AArch64 and ARM providers. Raw C++ entry and
host bridge conventions remain separate. Direct and indirect transfers can have
different parameter lists. Indirect bounds/null/type checks and cold-target
materialization occur before retiring the outgoing logical frame. Defined Wasm
imports resolve to a typed target; only host leaves use a tail-entered adapter
whose argument buffers outlive the retired Wasm frame.

The ordinary installed LLVM 23.0.0git X86 provider miscompiled guaranteed tail
calls with stack arguments. The task includes a minimal executable regression and
[a backport](toolchain/x86-tailcc-stack-arguments.md), but that patched old provider
has **not** yet been rebuilt and qualified. Both positive product builds below
use ROS's existing stable 23.1.1-uwvm-ros.7 provider. Unqualified older X86 providers
retain the prior typed convention unless an explicitly qualified backport macro
is supplied; unsupported tail prototypes fail emission rather than run corrupt
code. The stable ROS provider already contains the relevant LLVM fix.

- r79 CLI tests passed 144 ordinary full/lazy/lazy+verification and 72 ROS full
  checks, including 24 stack arguments, vector parameters/results, cold large
  functions, dynamic indirect tables and precise retired-frame diagnostics.
- r80 passed 180 checks across all four tier combinations and three stack
  policies. Tailcc public wrappers tail-enter their core. Normal core entry owns
  its logical frame; OSR inherits the interpreter frame, retires it before a
  native tail, and restores the suspended owner's expectation only on return.
- r80's separate OSR test passed 12 direct/indirect result and diagnostic checks.
  Successful runs require a nonzero actual OSR-ready counter; interpreting the
  entire test does not count as native OSR acceptance.
- r80 actual AArch64 LLVM JIT execution under QEMU passed both products' 96 pure
  plus 96 integrated validation cases, million-activation self/mutual tails, and
  direct/indirect cross-module/host concurrent runtime fixtures under instruction
  and unwind policies. This covers one target ABI profile, not the final matrix.
- r80's v22 serializer/product isolation passed 64 signed/unsigned invocations.
  The current v23 ABI additionally changes multi-result typed entries as below;
  r81 passed all 68 updated serializer/product isolation invocations.

The r81 implementation forwards an explicit trailing packed result address for
multi-result typed entries, returning native void. It avoids LLVM's forbidden
implicit aggregate-to-sret conversion at a musttail edge. The surviving non-tail
caller owns that buffer; raw wrappers forward their checked external result span.
Scalar/vector single-result entries retain register returns. Cache ABI v23 uses
`typed-call-abi=qualified-tailcc-explicit-tuple-buffer-v3`.

The new 31-value mixed scalar/vector/reference result cases passed 24 ordinary
full/lazy checks, 30 tiered checks and 12 ROS checks. The runtime fixture passed
16 ordinary tiered and four ROS full direct/indirect import/host/concurrent cases,
including unaligned externally guarded result buffers. Actual r81 AArch64 LLVM
JIT QEMU runs passed direct/indirect host and cross-module tuples under both stack
policies in both products. X86 emitted objects have native tail jumps and CFI,
with no calls on unwind success paths (cold nonreturning guest traps remain).
r81 tuple OSR testing
correctly failed its **migration assertion**: the old interpreter emitted no OSR
polls for multi-result functions. r82 removes that restriction, sums checked
result widths and reserves the complete result span even in a tail-only body;
r82 passed 36 actual direct/indirect OSR checks across one, two and 27 mixed
results, including retirement and post-return error stacks. Both complete module
import/export scans and the 45 checker semantic tests passed again.

Performance evidence is bounded: six actual mmap memory-loop bodies are identical
between r80 and r81 when using the same stable LLVM provider, with no new guard,
bridge or instruction-stack operation under unwind. The prior r74-to-r76 ABI
comparison also passed all six bodies on its original provider. Comparing across
LLVM provider versions changes induction-variable instruction selection, so that
is not recorded as an identical-body result. Three timing runs, including the
matched-provider r80/r81 run, still have overly wide confidence intervals; throughput non-regression is **not yet qualified**.

r79/r80 evidence was SHA-verified into the local ignored
`build/wasm3-evidence/archive-tmpfs-tail-r79-r80.tar.gz`. Current development uses
executable tmpfs inside the same 64 GiB/no-swap/16 E + 4 P cgroup because the
remote work filesystem has no free user space. tmpfs is volatile and newer
results must be archived before reboot.

## Memory64 address and execution development (r83–r111)

This is **internal implementation and focused qualification, not an enabled,
end-to-end memory64 feature**. The current parser memory record and the Wasm
instruction translators still select memory32. Memory64/table64 metadata,
initializer/import handling, address-typed operand validation, scalar/SIMD/bulk/
atomic dispatch, CLI gates, and all requested modes remain to be connected.
No memory64 CLI switch is advertised as working on the strength of these tests.
The old `validation/standard/wasm1p1` and `wasm2` directories remain identical to
HEAD in both repositories; new pure validation code lives exclusively in wasm3.

- `wasm3/address_limits` implements Core 3 u64 limits, i32/i64 address flags,
  per-kind bounds (memory64 up to 2^48 pages, table64 up to 2^64−1 entries),
  shared-memory flags/maxima and independent proposal gates, import matching and
  mixed-address copy length selection. The parser's existing shared-memory
  decoder now consumes this implementation with memory64 disabled and legacy
  binary width. r84's ASan/UBSan shared-memory suite passed both products,
  including 1,060 parser checks and the initializer/import/data/grow cases.
- `wasm3/memory_immediate` retains u64 offsets transactionally. The existing
  memory32 wrapper proves the offset fits before narrowing; it keeps the four-byte
  memory32 bytecode ABI. Both r85 suites passed 4,745 limits/memarg checks with
  inaccessible-page input boundaries. Forty complete memory64/table64 binary
  limits modules per product agreed with Wasmtime 48.0.2 compilation (including
  imports, extreme legal limits and malformed/truncated/u65 encodings).
- r93's address-aware `read_memory_argument64` validates an index before invoking
  its type resolver, checks the selected memory32/64 offset width and natural
  alignment, and commits the input cursor only after all checks. All current
  memory32 validator/interpreter/JIT callers share this implementation through
  the compatible wrapper. Both products passed 83 focused ASan/UBSan checks and
  the existing **single no-combine/no-delay** multi-memory configuration in both
  uncached and register-ring execution. This is not the final combine/delay matrix.
- LLVM's address emitter now handles an i64 address on an ISA32 target without
  narrowing before its carry and native-range proof. The full memory32 mmap path
  remains unchanged. The address-decision fixture passed 294 configurations and
  636,510 boundary/random cases both before and after O3: ordinary r84 used the
  installed provider; ROS r85 used bundled LLVM 23.1.1-uwvm-ros.7. Those decisions
  ran natively; they were not cross-target actual linear-memory accesses.
- `uwvm_int/optable/memory64` adds integer widths/sign extension, f32/f64 and full
  v128 loads/stores with i64 addresses/u64 offsets. It preserves the 65th sum bit,
  pins moving allocator memory, and keeps mmap accesses free of pin/lock calls.
  Failed cross-page stores probe before writing. The handlers cover byref,
  uncached musttail, merged/separate rings, one-slot and partial-cache layouts.
  r87 replaced a sign-mask/conditional-move sequence with a single sign-extending
  load; r89 repaired the selector's missing v128 cursor specialization.
  Both products passed 152 integer and 459 floating/vector checks on each of mmap
  and single-thread allocator. Floating tests include sNaNs, signed zero, endian
  layout and failed-store prefix preservation.
- `shared/wasm_memory64` and the interpreter size/grow handlers retain i64 results
  and reject high deltas before native conversion. Concurrent growth captures
  the returned old size under the backend lock. Strict and fail-fast allocation
  policies remain distinct. Both r91 products passed 192 mmap and 190 allocator
  size/grow checks. The mmap count includes concurrent old-size publication.
  r92 also fixes the JIT's generic page-maximum conversion to compare before
  casting to size_t, and supplies typed i64 size/grow fallback bridges. Compile-time
  boundary cases cover the ISA32 `2^32+1 pages` truncation hazard.
- Actual x86-64 disassembly passed **510 integer**, **175 floating/vector**, and
  **8 size/grow** handler specializations per product: tail variants jump to their
  successor; successful loads/stores and mmap size have no helper calls. Signed
  narrow integer loads additionally require a `movsx` memory instruction. The
  control-flow audit treats only explicitly qualified non-returning trap helpers
  as terminal; it does not hide arbitrary helper calls.
- Ordinary ARM32 and AArch64 integer/floating/vector tests passed under QEMU with
  both memory backends; ARM32 size/grow passed both backends. ROS AArch64 passed
  all three suites and both backends. The initial AArch64 allocator negative
  check omitted the target's SIGTRAP termination signal; its corrected rerun
  passed. Musttail was neither removed nor rewritten. Big-endian and full target
  coverage for these new handlers remain pending.
- r92's bundled-LLVM fixture executes **actual memory64 native load/store code**,
  retaining the production address checks, mmap store preflight, and terminal
  trap-call ABI. Both products passed **112 configurations / 692 checks** before
  and after O3, including real FP/NaN/vector bytes, u65 and high-address failures,
  unmodified failed-store prefixes, and generated i64 size/grow bridge calls.
  All 112 saved functions in each object have zero calls on successful access
  paths. Its trap sink checks diagnostic arguments and terminates the forked
  probe; this does not qualify whole-VM trap reporting or unwind. ROS used its
  bundled LLVM; the installed llvm-objdump only decoded the resulting object.

- r94 specializes the immutable standard 64 KiB page store policy once at
  translation. Actual x86-64 disassembly removes the dynamic flag/page-shift
  reads and variable shift while retaining carry/reservation and cross-page
  preflight checks. Both products passed 561 integer and 200 FP/vector musttail
  handler audits; successful paths contain no helper calls. Ordinary ARM32 and
  AArch64 mmap FP/vector checks also passed. r95 adds nonstandard 4096-byte and
  byte-sized pages (including a partially committed host page): ordinary passed
  541 mmap and 459 allocator checks. This is instruction evidence, not a new
  throughput non-regression claim.
- r95 adds full-width copy/fill primitives, mixed-width interpreter bulk handlers
  and typed LLVM bridge entry points. Both ranges are checked before pointer
  narrowing or mutation; aliases use memmove and one allocation pin, distinct
  relocating memories use a consistent pin order, and mmap does not acquire a
  pin. The interpreter preserves mandatory tail dispatch and the existing bulk
  operand-flush convention. Both products passed 232 mmap and 224 allocator
  checks, including successful sparse addresses above 4 GiB, zero endpoints,
  both overlap directions and untouched destinations after traps. Ordinary
  ARM32 and AArch64 passed these bulk cases on both backends under QEMU.
  These entry points are not yet selected by Wasm memory64 frontend dispatch.
  r96's actual generated LLVM calls passed all six mixed-width signatures and
  116 checks before and after O3 in both products; sparse >4 GiB accesses and
  exact terminal-trap arguments are included. r97's moving multithread allocator
  passed 113 checks in each JIT optimization mode in both products. Its concurrent
  copy/grow case uses disjoint byte ranges while copying in both directions.
  Both interpreter suites passed 233 mmap / 224 single-thread allocator / 225
  multithread allocator checks. Actual mmap native code audits found only libc
  transfer and cold trap calls in six interpreter and six JIT bridge bodies;
  interpreter dispatch remains musttail. This does not claim a throughput result.
  Memory64 memory.init and bulk host-provider dispatch remain pending.

- r98 adds shared u64 atomic immediate decoding and address-aware validation.
  All 67 threads opcodes retain exact natural-alignment requirements; memory32
  offsets are checked before narrowing, indices before memory lookup, and all
  input cursor changes are transactional and annotated. Both ASan/UBSan suites
  passed 5,337 checks. All 265 complete mixed-memory32/memory64 threads modules
  agreed with Wasmtime 48.0.2 compilation, including u64-maximum offsets, invalid
  memory32 offsets and over-alignment. Both module import/export audits passed.
  These are new syntax/decoder tests; the uwvm Wasm frontend is still memory32.
- r99/r100 qualify interpreter memory64 atomic load/store handlers: both
  repositories pass 106 checks per mmap/allocator backend. All 408 native atomic
  handlers retain musttail and have no successful-path helper calls or redundant
  fences; every store uses one sequentially consistent native atomic. The 561
  ordinary load/store success prefixes remain instruction-identical to r94.
  ARM32 and AArch64 execute both atomic backends under QEMU successfully.
- r101/r103 share LLVM atomic alignment/load/store lowering across i32/i64
  addresses. Both repositories pass 168 actual MCJIT configurations and 1,284
  checks each at baseline/O3, including in-bounds misalignment and 33/65-bit
  carry diagnostics. Saved native objects have no successful-path helper calls
  or extra fences. The ordinary FP/NaN/vector fixture also passes 112 configs /
  860 checks at baseline/O3 in both repositories. r102 exposed a fixture error
  that confused the memory32 diagnostic carry with bit 65; r103 corrects it.
- r104/r105 add memory64 RMW handlers and shared LLVM RMW lowering, covering all
  49 encodings. Interpreter tests pass 2,358 checks per memory backend/repository;
  1,624 actual musttail handlers have no successful-path helper calls. ARM32 and
  AArch64 pass both backends under QEMU. LLVM tests pass 588 actual configurations
  and 5,184 checks per baseline/O3/repository, including concurrent updates,
  truncated expected/replacement operands, zero-extension and failed-store
  immutability. Native objects contain the required atomic instruction with no
  auxiliary fence. Both memory32 integrated RMW regressions pass: each of four configurations
  accepts 196 and rejects 560 cases, including feature gates and indexed memory.
- r106/r107 add memory64 wait/notify through the existing VM execution/wait
  domain. Carry is rejected before comparing, registering or notifying a key;
  allocator pins end before sleeping. Both interpreters pass 58 mmap, 18
  single-allocator (notify/nonshared rejection) and 52 threaded-allocator checks.
  Actual LLVM bridge calls pass 29 mmap checks each at baseline/O3 in both repos;
  threaded allocator also passes 26 checks per baseline/O3/repository. Sparse addresses above 4 GiB,
  wait32/wait64 aliases, growth during waiting and host cancellation are covered.
  r106 identified same-signature bridge symbol collisions and a truncating
  memory32 diagnostic helper. r107 uses explicit operation discriminators and
  preserves all 64 static-offset bits in the cold trap ABI. Both module surface
  audits pass through r108. ARM32/AArch64 execute mmap, single-allocator and
  threaded-allocator wait handlers successfully at r108. Both current memory32
  integrated JIT RMW regressions pass instruction/unwind policies, and wait/notify
  passes 15 accepted / 41 rejected cases under both stack policies. The wait
  handler fixture now forces indirect dispatch: the prior compiler devirtualized
  notify away, leaving only two actual musttail bodies for the three-op audit.
  r112/r113 force actual dispatch and qualify all three musttail bodies in both repos.
- r109/r111 replace concurrently mutable data begin/end pairs with an atomic
  dropped flag and immutable module-owned payloads. Both memory32 init backends,
  including imported JIT memory, now acquire a consistent live/empty snapshot.
  Module teardown must still drain the existing host execution leases. Both
  repositories pass 524,288 concurrent reads under ASan/UBSan and TSan, including
  retained snapshots, repeated drop and constexpr/trivial-relocation checks.
  The initial fixture lacked runtime build macros; the current libc++ also lacks
  atomic_ref<const bool>::load, so the implementation uses a read-only bool
  atomic_ref alias without depending on that library extension.
- r110/r111 add interpreter and LLVM memory64 init primitives with an i64
  destination and i32 source/length, as required by Core 3. Both repositories pass
  48 mmap / 46 single-allocator / 46 threaded-allocator interpreter checks. Actual
  LLVM calls pass 24 checks at both baseline/O3 per repository. Full error metadata,
  zero-length endpoints, dropped/active data, >4 GiB sparse writes and unmodified
  destinations on failure are covered. LLVM threaded-allocator passes 23 checks per baseline/O3/repository. Native
  interpreter/bridge assembly has only libc transfer and cold trap calls; the
  interpreter retains musttail. ARM32/AArch64 pass mmap and allocator at r112.
  Fresh whole-JIT imported-memory regressions pass in both repos at r113.
  These are execution primitives; memory64 frontend dispatch, full VM stack
  reporting and the final mode/platform matrix remain unqualified.

- r114 adds declared memory address metadata and complete standalone Core 3
  instruction typing for scalar, SIMD, atomic, size/grow, init/fill and mixed
  copy operands. Both repositories pass 3,297 guarded ASan/UBSan checks, including
  2,928 complete modules accepted/rejected consistently by Wasmtime 48.0.2.
  Import/local selection, every atomic opcode, all SIMD memory encodings, wrong
  operands, stack polymorphism and truncated u64 immediates are covered. These
  tests explicitly supply the address type in the semantic context; the binary
  memory64 parser and integrated translators remain pending. The original
  wasm1p1/wasm2 validator folders remain unchanged.

- r115/r117 preserve declared memory limits in u64, saturate native maximum
  resources only after comparing at full width, and reject unrepresentable native
  minima. Address type joins sharedness in import matching; the current external
  provider ABI remains memory32. Metadata formatting and native limit tests pass
  ASan/UBSan in both repos and ARM32/AArch64 under QEMU in the ordinary repo.
  The initial formatter fixture expected alphabetic booleans; r117 correctly
  checks FastIO's established numeric format. Fresh JIT imported-memory and
  interpreter multi-memory regressions pass both products at r116.
- r118 adds all 22 memory64 SIMD interpreter operations, including every lane,
  extended/splat/zero loads and full/lane stores. Both repositories pass 681 mmap
  and 671 allocator checks; 760 native mmap handlers retain musttail and have
  no successful-path helper calls. ARM32/AArch64 execute both backends under QEMU.
- r119 adds u64-address SIMD native JIT bridges with explicit opcode relocation
  identities. Both repositories pass 444 actual generated configurations and
  1,998 checks each at baseline/O3, covering all lanes, software/partial-protection
  direct access, bridge calls, full trap metadata and failed-store immutability.
  Direct native access bodies contain no successful-path helper calls or extra
  synchronization. Both threaded-allocator runs pass 148 configurations / 666
  checks at baseline/O3. All 561 existing scalar memory64 success paths retain
  instruction-identical native code against the r100 baseline.
  This still does not enable the memory64 binary frontend or integrated compiler
  dispatch; those require the remaining parser/initializer/translator work.


- r120-r123 complete the initializer's address64 selection: native mmap reserves
  the existing memory64 partial-protection domain; declaration limits remain u64
  and are checked against 2^48 pages. Active data offsets use the selected memory's
  i32/i64 type, including imported aliases and extended/global expressions. The
  entire offset/span is checked before narrowing to a host pointer.
  Both repositories pass 17 mmap UBSan checks (including a sparse write above
  4 GiB) and 16 checks per allocator under ASan/UBSan. The parser passes 532
  guarded checks and 460 complete Wasmtime modules per repository. ARM32/AArch64
  run mmap and allocator successfully under QEMU; fresh whole-JIT imported-memory
  regressions also pass both products. The initial r120 compile errors were a
  missing forward declaration and a test alias colliding with the C++ module
  keyword. r121/r122's native ASan runs labelled mmap actually selected the
  allocator by repository policy: r123 fixes the runner and asserts the selected
  backend. These remain semantic-context tests, not binary memory64 VM support.


- r124/r125 add native scalar memory64 JIT fallback calls. All 23 scalar memory
  opcodes preserve integer extension and floating payloads through an unsigned
  i64 carrier, including targets whose C ABI extends i32 arguments. Full u65
  address checks precede native pointer construction and writes; moving
  allocations remain pinned. Both repositories pass 46 actual configurations /
  322 checks at baseline and O3 with mmap and threaded allocators. All 16 unique
  native mmap scalar bridge bodies contain no successful-path helper calls or
  extra hardware synchronization. The first build needed an explicit FastIO
  string-view constructor; ROS's link/post-fingerprint jobs were recovered after
  ENOSPC, without changing production source. This qualifies the fallback ABI on
  native x86-64. At r126 both repositories also execute the same 46 configurations /
  322 checks at baseline/O3 through actual RISC-V64 MCJIT under QEMU, using the
  independently hashed bundled target LLVM23 libraries. The first cross run
  exposed a missing LLVM native asm-parser initialization in the fixture; the
  corrected fixture passes. Integrated frontend dispatch and the remaining
  native ABIs are still pending.

- r127–r130 connect all 23 scalar memory instructions and memory.size/grow
  through the integrated int/JIT validators and emitters in both repositories.
  Wide offsets stay u64 and addresses/page counts use i64; memory32 retains its
  existing compact bytecode and access path. LLVM replay rechecks the selected
  memory32 offset bound before native ABI narrowing. memory64 size and grow(0)
  retain direct size loads on mmap, with ordered loads for shared memories.
  Both products pass 138 scalar executions, ten-byte offset compilation, wrong
  address/result types, and grow limit/high-bit/failure results. Int passes all
  12 combine/delay configurations, each with uncached and scalar register-ring
  execution. Both full-JIT runtimes pass the focused native mmap test. r127's
  first fixture needed a mutable view of owned runtime storage; r128/r129 then
  exposed its legacy validator policy selection. r130 explicitly selects the
  Core 3 scoped policy. No legacy validator folder was modified.
- r131 connects memory.copy/fill/init in both integrated compilers, with separate
  source/destination address types and Core 3's mixed-copy i32 length. Full-width
  bridge proofs precede native narrowing; module-relative data identities keep
  relocations cacheable. Both int configurations and both full-JIT runtimes pass
  four width combinations, 14 transfer checks per combination, overlap in both
  directions, empty endpoints, data.drop, and 48 wrong-operand modules. The cache
  ABI moves to v24 and records the u64 declaration/immediate contract; cache
  rejection checks and provider/moving-allocator regressions are being expanded.
- r132 qualifies the v24 cache contract: both products pass 21 memory-backend
  serialization/replay checks; product isolation and obsolete-policy rejection
  pass 72 fixture invocations. These are object-cache tests, not native execution.
- r132–r134 connect all 22 SIMD memory instructions in both compilers. Each
  product passes 222 executions covering 74 opcode/lane combinations in four
  interpreter layouts, including vector ring pressure. Full JIT passes mmap and
  moving-allocator execution. r133 reserves disjoint result-ring capacity before
  scalar/SIMD loads; r134 fixes address residency when vectors use the operand
  stack by selecting a memory-only handler with the same tail-call ABI. Enabled
  vector rings retain their cached path. Scalar pressure tests also pass all
  four layouts. Actual executable assembly audits pass 467 scalar handlers,
  456 SIMD handlers and 10 size/grow handlers per product: musttail dispatch and
  no successful-access helper calls (grow may call its allocation slow path).
- r135 cross/allocator runs were interrupted by tmpfs exhaustion during output
  compression/linking. They are not passes. Earlier completed artifacts and
  source snapshots were archived locally and SHA-256 verified before pruning.
- r136 qualifies scalar JIT allocator execution with explicitly separate
  instruction/unwind policies and retained IR. Earlier scalar/SIMD/bulk fixture
  binaries ignored the runner's policy argument; their execution results remain
  useful, but do not qualify two independent stack-reporting policies. Integrated
  ARM32/AArch64 checks are being rerun after the storage failure.
- r137 wires all FE atomics to selected memory address types, preserving the
  memory32 bytecode/native ABI and full u64 immediates for memory64. Native JIT
  fallback address carriers are independent of value carriers, and their bridge
  symbol identity includes address width. A mixed-declaration, ring-pressure and
  overflow-trap fixture initially needed an explicit u64 declaration in its
  oracle and a VM wait-domain scope around direct interpreter calls. r139 then
  passes 536 executions over all 67 FE opcodes in four interpreter layouts per
  product, plus 66 wrong-address modules. Both r138/r141 full-JIT mmap and moving
  allocator runtimes pass instruction and unwind independently, including u65
  overflow traps with function-stack diagnostics. r143 moving-allocator int also
  passes ASan/UBSan. Actual interpreter binaries qualify 280 atomic load/store
  and 980 RMW handlers per product: musttail and no successful-path helper calls.
- r141 inspects signed cache objects emitted by the actual full JIT. All 252
  native mmap atomic functions per product retain native atomics and CFI;
  instruction has two successful-path bookkeeping calls, unwind has zero.
  These are codegen checks, not throughput measurements. Private fixture caches
  are explicitly enabled only after setup. r140's first cache-capture helper
  used the wrong string assign overload; r141 corrects it.
- r136 ordinary integrated scalar/bulk/SIMD passes all twelve ARM32/AArch64 and
  allocator/mmap suite combinations. The ROS rerun is still finishing. r143
  atomic QEMU passes mmap on both ISAs; its single-thread allocator selection
  correctly fails shared-memory initialization. The runner now selects the
  concurrent allocator for shared atomic suites, and both products are rerunning.
- r144 makes elem.drop visibility atomic while retaining immutable function-index,
  canonical-funcref and externref pointer pairs. Interpreter and JIT table.init
  take one acquired snapshot; initializer reads use the same visibility helper.
  Both products pass 525,312 element snapshots and the existing 524,288 data
  snapshots under ASan/UBSan. TSan initially encountered a pre-main ASLR collision
  under container seccomp; bounded retries retain all logs and never retry races.
  Full translated element/drop regressions and TSan are still under test.
  These integrated fixtures explicitly set owned declaration metadata after
  initializing a memory32-encoded module. Binary memory64 parsing and the CLI
  feature gate remain closed until threads and every remaining consumer are
  qualified; these passes do not claim end-to-end binary memory64 support.

Reproducible focused runners include `run_wasm3_address_limits.sh`,
`run_wasm3_memory_validation.sh`, `run_memory64_integer.sh`,
`run_memory64_values.sh`, `run_memory64_pages.sh`, `run_memory64_cross.py`,
`run_address_limits_wasmtime.py`, `check_memory64_access.py`, and the saved-object
codegen checks. All compilation/execution occurred on SSH Linux inside the same
64 GiB, no-swap, `0,2,4,6,16-31` cgroup. No local compilation or runtime tests
were performed. r131 changes the cache ABI schema to v24; memory32 serialized instruction
widths remain unchanged. General throughput non-regression and
final acceptance are still unqualified.

## Binary memory64 integration and segment lifetime (r144–r151)

Both products now parse memory64 declarations behind independent
`--wasm-feature-enable-memory64` / `--wasm-feature-disable-memory64` switches.
The flag also selects the Core 3 u64 limits/offset grammar for memory32, while
memory32 semantic ranges remain bounded. Shared memory64 additionally requires
`threads`. Wide limit diagnostics retain full u64 values; legacy diagnostics keep
their existing representation. Initializers and all integrated code validators
recheck the policy, including parsed modules reused with a stricter/default policy.
The wasm1p1/wasm2 pure validator folders remain HEAD-clean.

Ordinary lazy interpreter structural scans and LLVM direct-callee grouping now
consume u64 memory offsets. Atomic callee scanning recognizes the complete FE
immediate before finding subsequent calls. These changes are still undergoing
actual CLI lazy/tiered qualification; full-mode results do not establish lazy-mode
acceptance.

r147 converted scalar/SIMD/bulk/atomic fixtures to **actual binary memory64
records**, removing their previous post-parse declaration mutation. Both products
passed all 67 atomic opcodes (536 executions per layout, four layouts) in
extra-heavy combine/heavy delay interpreter builds. Both passed scalar full-JIT
instruction and unwind runs (138 executions / 23 memory opcodes plus size/grow and
register-pressure cases). The initial scalar-JIT harness invocation aborted because
its requested IR output directory was absent; reruns with the directory created
passed, and the failed logs remain preserved. Both r148 full-JIT atomic fixtures
also passed 536 executions; ordinary r148 includes both runtime backends for the
forthcoming CLI mode checks.

The r146 ARM32/AArch64 integrated atomic matrix passed both products and mmap /
concurrent-allocator backends. The r136 scalar/SIMD/bulk matrix passed both products
on ARM32/AArch64 with mmap / allocator. These older QEMU fixtures supplied explicit
semantic address metadata and are not binary-memory64 frontend qualifications.

The `elem.drop` publication fix uses an acquire/release dropped flag and immutable
payload pointers. Existing borrowers retain their bounded snapshot until module
lifetime ends; table.init sees either the original segment or an empty segment.
r144 ASan/UBSan and r146 TSan passed both products: 525,312 concurrent element reads
per run, all three payload representations, alongside 524,288 data-segment reads.
TSan ASLR startup failures were retried only when recognized, with logs retained.
r145 integrated table.init/elem.drop passed interpreter and full JIT, including
instruction/unwind diagnostics. r146 implicit-drop and module-export checks passed.

The first r148 combined parser/formatter ASan builds exceeded the shared 64 GiB
cgroup during parallel compilation (one OOM kill); the other expensive build was
stopped. No runtime result is claimed for those attempts. r151 separates formatter
coverage into actual CLI checks while retaining guarded parsing, policy and full-width
numeric diagnostics under sanitizers. Qualification is still in progress.

r151 binary parser/initializer policy suites passed ASan/UBSan in both products:
2,161 guarded parser cases each, u64 diagnostics, reused-module initializer and
validator gates, and independent CLI ownership. r152 ordinary CLI passed 307 runs
across interpreter full/lazy/lazy+verification, JIT full/lazy/lazy+verification with
instruction/unwind, all four tier combinations in both lazy modes, and standalone
validation. ROS JIT full instruction/unwind and validator passed 77 product runs;
Wasmtime 48 passed the same 57 fixtures (134 combined runs). The original upstream
`test/core/memory64/load64.wast` contributes 37 ordered assertions in its original
module and 47 invalid binary modules; 13 malformed **text-format** cases are
explicitly excluded because the VM consumes binary modules. Upstream SHA-256 and
case logs are retained. These are focused mode checks, not the full Core 3 suite.
Both CLI products also passed actual cold/warm signed-cache reuse, refusal after
memory64 is disabled, and rendered diagnostics above 2^32 and 2^48. Actual tiered
OSR under memory64 is not established by these short mode tests.

### r153–r159 incremental qualification and table64 work

ROS r153 combined CLI (interpreter full, LLVM full instruction/unwind, standalone
validator, Wasmtime 48) passed 144 memory64 fixture runs. Both r154 products passed
ARM32 NEON QEMU integrated atomic and SIMD fixtures using **actual binary memory64**
declarations, on mmap and concurrent-allocator backends. Both r153 interpreter
stress assembly checks passed 280 atomic load/store handlers with musttail and no
successful-path auxiliary call.

r152 native interpreter timing used 20,000,003 iterations, 15 interleaved rounds,
P-core 0, and three independent-load kernels. Ordinary ratios to HEAD were 0.9104,
0.9372 and 0.9641 for aligned scalar, cross-page scalar and SIMD; all six comparisons
(including indexed/current syntax) passed the 1.05 upper-confidence bound. ROS
**failed** the two scalar comparisons: ratios 1.1088 [1.0876, 1.1261] and 1.1241
[1.0968, 1.1570]. ROS SIMD passed. Actual streams have unchanged handler counts and
byte sizes; scalar successful load code is unchanged and stores have fewer
instructions. Same-process stream relocation reproduced only about 3% variation.
An r155 ELF memory64-handler grouping experiment did not resolve the regression
and was **reverted**, with failed timing retained. ROS scalar throughput acceptance
remains open; these results do not qualify the complete performance matrix.
The task-owned ROS interpreter HEAD baseline restores the original LLVM version
header (no LLVM overlay is required for this interpreter-only comparison).

r157 table64 parser/initializer/standalone-validation suites passed ASan/UBSan in
both products: 1,106 guarded binary cases and 368 typed instruction cases each.
They cover independent table64 control, local/imported u64 limits, ten-byte
truncations, mixed table-copy widths, indirect/tail-indirect selector types,
i64 active element offsets and rejection beyond 2^32 without truncation.
Pure validator changes remain confined to wasm3, including its new exported
`table_validation.h/.cppm`. Cache schema is now v25 and names the widened table
metadata; current cache migration checks have not yet been rerun.

r158/r159 interpreter table64 handlers and typed translation are in development.
They retain the existing flushed-reference ring convention and musttail dispatch;
indirect-call runtime bridges now preserve a full-width selector. The first
integrated test build failed in its fixture's attempt to mutate a const view of an
owned runtime table, not in a product template; r159 corrects that fixture access.
At that snapshot table64 execution was not yet qualified and LLVM lowering was not wired.
The subsequent r160/r161 work below supersedes that development state.

During r157 dispatch the test preflight detected `memory.swap.max=max` despite
Docker's configured equal memory/memory-swap limits. No new build started under
that condition. Reapplying the 64 GiB Docker limits restored the actual cgroup
swap limit to 0 and cpuset remained `0,2,4,6,16-31`. The earlier r148 OOM count remains
one; no later OOM occurred. A separate ROS r154 QEMU job was in flight during this
configuration drift, so its passing results are functional evidence; repeat its
resource qualification under a continuously checked cgroup before acceptance.

### r160–r161 table64 interpreter and LLVM lowering

Both integrated interpreters passed the table64 binary fixture with ASan/UBSan:
ordinary r159 and ROS r160, in uncached, merged two-slot and merged one-slot ring
layouts. The ROS first build exposed ordinary-only mixed-engine table-view refresh
hooks; ROS full interpreter now follows its existing no-JIT-view mutation policy.

JIT validation/emission selects exact i32/i64 operand and result ABIs from each
table declaration. Mixed-width copy uses the smaller address type for length;
init retains i32 source/length. Every bridge proves unsigned full-width ranges
before narrowing to a host index, including zero-length operations. Indirect and
tail-indirect LLVM lowering compares at max(table address width, host pointer
width), then derives the target pointer only in the checked successor block.

Both r161 complete runtime objects compiled with interpreter and LLVM enabled.
The integrated JIT fixture passed **200 executions per stack policy per product**,
including both reference kinds, all four source/destination widths, overlap,
segment drop, growth failure limits, >2^32 indices and unsigned-maximum lengths.
Trap children require the actual table-OOB diagnostic and module call-stack entry;
this checks instruction and native unwind separately. The first fixture attempt
incorrectly assumed SIGABRT and redirected only fd 2, while the VM owns a duplicated
log descriptor and uses fast termination. Corrected fixtures reopen that descriptor
and require the actual diagnostic, stack entry and signal termination.

Source/dependency digests and runtime-object hashes were verified for fixture reuse.
Both actual CLIs are linked against those runtime objects. Focused CLI mode and signed-cache checks passed **384 ordinary / 76 ROS runs**,
including the reference/validator rows. Both ARM32 NEON QEMU interpreter fixtures
also passed in all three test stack layouts (concurrent-allocator build). A pinned official subset (unchanged
call_indirect64, table_size64 and table_copy_mixed modules) plus new table64 trap,
indirect-argument and tail-call cases passed 17 Wasmtime 48 runs. The three-file
subset is recorded explicitly; this does not claim the complete official suite.

### r162 recoverable table growth and actual code generation

`try_grow_table_elements` uses the existing native strict allocator and retains
fast_io vector ownership/alignment. Allocation failure leaves the allocation,
size, capacity and contents unchanged. Zero growth never allocates. Host byte
limits are checked before allocation; successful reallocation publishes the new
vector pointers only after storage exists. Both narrow and wide interpreter
handlers and LLVM bridges return their width-specific -1 on allocation failure.
No ordinary memory access path is changed.

Both native ASan/UBSan table-growth tests passed real RLIMIT_AS-induced allocation
failure, preserved references/ownership, resource restoration and successful retry,
zero growth, byte-count overflow and capacity reuse. Both extra-heavy-combine /
heavy-delay table64 fixtures passed. Both complete JIT runtime fixtures again passed
200 executions per policy, with actual trap stack diagnostics.

Actual x86-64 interpreter disassembly passed **36 tail handlers per product**:
all seven operations retain indirect musttail dispatch and no ordinary return;
table.get/size successful paths have no helper calls. Actual signed-cache JIT
objects passed **144 function checks per product**: eight real binary modules,
nine functions, both stack policies. Native unwind has only the semantic operation
bridges; instruction mode has exactly two additional frame-maintenance calls.
All objects retain CFI. The inspection keys include each module's original Wasm
hash because the fixture intentionally reuses its display name across instances.
This is assembly evidence, not a throughput acceptance claim.

Both products passed the updated **76 cache product/schema/signature invocations**,
including independently rejected v24 table layouts. Both whole-tree module import
scans passed after adding the strict allocator dependency. The unchanged old pure
validator directories remain HEAD-clean.

Task-owned tmpfs source snapshots r148–r160 were archived locally with a per-file
SHA-256 manifest and verified archive hash before pruning 73,807 regular files
(951,026,968 bytes). Current r161/r162 sources and runtime artifacts were retained.

### r163–r165 recursive type system foundation

Both products now contain the Core 3 recursive type representation and bounded
binary decoder, plus semantic validation and closed-type interning under
`validation/standard/wasm3`. These APIs are **not yet wired into the full module
parser, function-body validators, interpreter or LLVM emitter**. This is not a
claim of working GC, call_ref, exception references, new CLI switches or the full
Core 3 validator. The ordinary and ROS implementations are identical.

The decoder covers rec/sub/final, function/struct/array composites, packed fields,
nullable and non-null references, all Core 3 abstract heaps and signed-33 type
indices. It traverses bounded offsets and commits the caller cursor/output only
on success. Count checks precede allocation; malformed input cannot expose a
partial section. Abstract heap productions cannot use padded negative LEBs.

Semantic validation checks scope through the current recursive group, preceding
non-final single supertypes, function parameter contravariance/result covariance,
struct width, immutable field covariance and mutable field invariance. Exact
closed-group keys retain binder positions, group size, finality and supertype
identity. Per-module matching uses forest intervals. The VM-domain registry
interns closed external references across modules and uses stable IDs with compact
ancestor jumps; it is a builder requiring caller serialization and has not been
connected to VM lifetime/publication yet. No global singleton is installed.

Incremental Linux cgroup evidence (64 GiB, no swap, CPUs 0,2,4,6,16–31):

- r163/r164/r165: both products pass 2,370 ASan/UBSan guarded binary checks and
  50,190 semantic/matching checks, including a 50,000-type inheritance chain.
- r165: both products pass registry rollback/stable-ID checks, closed cross-module
  references and 100,000 randomized ancestor pairs without native recursion.
- r165: both products pass Clang/LLVM/QEMU runs of the binary and semantic/registry
  fixtures on ARM32 and big-endian s390x. IR, assembly, machine verification and
  execution logs are retained under `shared-r165/REPO/type-cross`.
- r164 ordinary and r165 both products agree with wasm-tools 1.259.0 and Wasmtime
  48.0.2 on 139 type-section payloads from pinned Core 3 type, type-rec, type-canon,
  type-equivalence and GC/type-subtyping files. 36 are complete original type-only
  module assertions; the remainder test only the original type-section bytes.
  Two inputs without a usable type section are explicitly skipped. This does not
  execute the GC programs. The text encoder rejects the official multiple-parent
  negative before producing binary; one reviewed equivalent binary module is used
  for that assertion, with original text, bytes and reason recorded in the result.
- wasm-tools release archive SHA-256:
  `3e9b374b4c7715b771b69bf0d65a337990ed4546ec5e97e01c0ff587dfc52160`;
  extracted binary SHA-256:
  `115d5986a8a1aeb112a5f2d98209c144f70c26188a5d41e266698a1e20de4ed1`.
- All new headers are exported through impl.h/impl.cppm. Both module dependency
  audits pass. Pure wasm1p1/wasm2 validator directories remain HEAD-clean.

Both r165 products additionally pass nine comparisons of the registry API against
**real Wasmtime import instantiation**: same closed group, projection/group-size
mismatches, finality, declared/undeclared subtyping, closed external references,
recursive variance and mutable field differences. This verifies the registry
model; UWVM's VM linker has not yet been switched to these IDs.

### r166 non-default local initialization foundation

`wasm3/local_declarations.h` decodes nullable/non-null typed locals as compressed
runs, retaining all unsigned counts and checking total locals plus parameters
before arithmetic. Zero-count type uses are still checked. The cursor/output are
transactional; the first body instruction is left unread. Lookup uses bounded
indices and handles zero-count runs. The initialization tracker stores only locals
actually assigned without default values and rolls back to control-frame entry
checkpoints at else/end, matching the Core 3 structured-instruction rules. It is
not yet connected to the interpreter/JIT/function validator.

Both products pass 25 ASan/UBSan guarded cases, including a compressed declaration
of 2^32-1 locals without allocating that many entries, count overflow, unknown
zero-count type uses and sparse scope rollback. The dedicated initialization pass
also agrees with Wasmtime on all six original module assertions in pinned Core 3
`local_init.wast` (SHA-256
`4a496f820a6863d429c772f4ac85018944fa96e1f1cec6df9fc9231c9795890d`). The fixture
checks initialization state only and rejects instructions outside its supported
subset with a separate failure; it is not advertised as a complete code validator.
The recursive type/registry ASan/UBSan regression and both module dependency audits
pass with these additions.

The four narrow table.grow handlers touched for recoverable allocation now also
annotate their IP/immediate/operand-stack cursor movements. Those additions are
comments only. The new type APIs do not change the executed memory hot path;
this observation does not resolve the earlier ROS scalar performance failure.

### r167 reference instruction typing foundation

The shared operand interface now validates call_ref/return_call_ref, ref.null,
ref.func type resolution, ref.is_null/ref.as_non_null and the null-branch typing
rules. Whole-value polymorphic bottom is distinct from a reference to heap bottom.
Branch label prefixes are reified even in unreachable code, avoiding accidental
numeric acceptance of a known reference. This is not yet a full VM frontend.
Both products pass the ASan/UBSan foundation tests, module dependency audits and
20 exact straight-line function bodies compared with Wasmtime 48.0.2.

### r168 direct JIT reference-null query

The actual full-JIT emitter now extracts the native reference-kind field from its
SSA integer and compares the tag directly. It uses the target data layout's
endianness and the native object's field offset/size. Payload and padding are
excluded; a null payload alone is not a null reference. No new memory guard or
synchronization is added. The existing bridge is retained for ABI compatibility,
but this lowering no longer emits its buffer or call.

Both products pass 200 table64 executions under each of instruction/unwind
policies. Signed-cache objects from the actual JIT contain 144 checked functions
per product: the table.get + ref.is_null function has one semantic call instead of
two. Instruction mode adds two frame-maintenance calls; unwind omits both and
retains CFI. Dedicated production-emitter objects also pass 2,304 tag/payload/
padding combinations on each of i386, ARM32, AArch64, RISC-V64, PowerPC32 and s390x
under QEMU, with no native helper calls. The cross-object fixture is not a complete
target JIT or exception-unwind test. The initial PowerPC32/s390x oracle build used
an incomplete Clang object backend; generating oracle IR with Clang and objects
with the paired LLVM provider fixed the test driver. Original failure logs remain.

### r169 aggregate/scalar-reference typing foundation

Shared GC rules cover struct.new/new_default/get/get_s/get_u/set, the array
construction/access/fill/copy/data/element operations, array.len, ref.i31,
i31.get_s/get_u and ref.eq. The rules check field bounds, packed/unpacked access,
mutability, non-defaultable fields, storage subtyping and segment declaration
bounds. In particular, array.copy does not equate i8/i16 merely because both
unpack to i32. The explicit GC policy rejects disabled operations before changing
the operand stack; it has not yet been connected to CLI feature selection.

array.new_fixed consumes only concrete operands and handles the polymorphic
unreachable remainder in constant time, so a count of 2^32-1 neither allocates nor
loops that many times. Both products pass the foundation ASan/UBSan run and module
exports audit, and agree with Wasmtime on 75 new aggregate/i31/reference-equality
function-body cases. These tests include real encoded type, body, data-count and
passive element declarations. They validate this supported typing subset;
GC allocation/execution and full-frontend integration are still unfinished.

### r170 reference casts and conversion typing

The shared rules now cover ref.test/ref.cast, any.convert_extern and
extern.convert_any, br_on_cast and br_on_cast_fail. Cast inputs may be siblings
within the same heap hierarchy; branching casts still require the declared target
to be a subtype of the declared source. Reference difference refines nullability
only. Polymorphic conversion inputs produce a non-null value in the required
any/extern hierarchy, never a numeric or unrelated reference bottom. Both products
pass the ASan/UBSan foundation tests and module audit, and agree with Wasmtime on
55 exact function bodies, including function-label branch prefixes and dead code.
Nested control flow and the complete VM frontend are outside this fixture's scope.

### r171 complete GC-prefixed immediate grammar

`gc_immediate.h/.cppm` provides the shared, transactional binary decoder for all
31 Core 3 0xfb subopcodes. It retains unsigned indices and signed-33 heap types,
checks the literal branch-cast flag byte, and commits the caller cursor once, only
after the complete immediate is available. The instruction-typing fixture now
uses this production decoder. Both products pass 1,524 guarded page-boundary cases
covering all subopcodes, truncated prefixes, padded opcodes, reserved cast flags,
maximum indices and failure cursor rollback. All new files are exported by their
folder's impl.h/.cppm; the wasm1p1/wasm2 pure-validator directories remain HEAD-clean.

With the unified decoder in place, both r171 products also pass all 150 reference,
aggregate, cast and conversion differential cases (20 + 75 + 55). Clang IR,
LLVM machine verification, object/assembly generation and QEMU execution pass for
the reference-validation and GC-immediate fixtures on ARM32 and big-endian s390x.
These targeted checks do not replace full-frontend, full-JIT or full-matrix testing.

r167-r169 evidence is archived as `build/wasm3-evidence/references-r167-r169.tar.gz`
(SHA-256 `6f427c794234aaebabcd952bf7d486fb237c6b8e741ab8892de003e12c7d19cb`).
Older r161-r169 src/test/tools/documents snapshots are separately archived with
per-file hashes in `archive-tmpfs-sources-r161-r169.tar.gz` (SHA-256
`f3869838a90015f58e2a337efad353918bb3e2be0519107a2cb378e7228d84ec`).
Only the 51,526 individually verified archived source files were pruned remotely;
r170/r171 source trees remain intact for subsequent snapshots. Do not use the
pruned source directories as a build base without restoring them first.

r170/r171 completed validation evidence is archived separately in
`gc-validation-r170-r171.tar.gz` (SHA-256
`23100616f50074b991534d6509cd987091af89f582f80d7c7713cc9042442e53`), with all 4,440
regular files checked against the saved per-file manifest. The archive excludes
the subsequently started ROS performance build and its timing results.

### r171 latest ROS focused memory timing

The current ROS source and task-owned HEAD baseline were rebuilt with Clang -O3
-march=native, no combine/delay, mmap, and the same checksum-checked harness.
All six paired comparisons pass the predeclared 1.05 upper-confidence limit in
15 interleaved rounds of 20,000,003 iterations on permitted P-core 0:

| Independent read kernel | Current legacy / HEAD | 95% interval |
| --- | ---: | --- |
| Aligned scalar | 0.974822 | [0.958636, 0.989480] |
| Cross-page scalar | 1.017079 | [0.990074, 1.046489] |
| Aligned SIMD | 0.923367 | [0.881965, 0.963033] |

The three indexed/legacy comparisons also pass. The regenerated fixtures first
passed 24 independent Wasmtime checksum executions. An initial invocation referred
to a removed old fixture path and exited before measurement; its empty result
directory is not treated as performance evidence. The successful result is in
`shared-r171/uwvm2-ros/int-performance-timing-v2`.

Inspection on E-core 16 resolves actual bytecode handler pointers and saves the
native assembly. The three legacy streams retain their 416/436/644 byte sizes;
25 common handler success prefixes are instruction-identical. The three changed
memory handler identities correspond to the specialized scalar store and SIMD
load/store paths; their emitted code is saved separately. These latest results
supersede r152 for this exact current-source profile, but do not establish the
cause of r152's regression or qualify the remaining combine/delay/per-platform
performance matrix. The earlier failed samples and reverted experiment remain
part of the evidence. No extra memory guard was introduced by the reference work.

The complete r171 focused ROS performance artifacts, including binaries, build
commands, source manifests, generated fixtures, samples and inspected assembly,
are archived in `ros-memory-performance-r171.tar.gz` (SHA-256
`57e4a1265d610b9776446f0d92521280018d70daa30346ee19853698952c320b`). All 55 regular
files were checked against the manifest; these remote artifacts were not pruned.

## Reference execution and tag lowering (r172–r176)

The independent `function-references` switch now gates actual `ref.as_non_null`
execution in the standalone wasm3 validator, inline interpreter/JIT validators,
and ordinary lazy scanner. `reference_policy.h/.cppm` are included/exported by
wasm3's impl files. A compile-time reference-bottom marker preserves unreachable
code's reference-only result; it cannot match an integer or vector. Local.tee
reifies its declared carrier. This slice supports the currently integrated
funcref/externref declarations, not arbitrary typed/non-null declarations or
complete function references. The old wasm1p1/wasm2 pure-validator folders remain
HEAD-clean in both repositories.

Normative rules: [validation](https://webassembly.github.io/spec/core/valid/instructions.html#valid-ref-as-non-null)
and [execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-ref-as-non-null).
The pinned official `ref_as_non_null.wast` also contains declarations/call_ref
outside the integrated frontend; the entire official file has **not** passed.

Each product passes 50 standalone/integrated validation cases and 1,538 native
JIT executions per frame policy, including null traps with call-stack diagnostics.
The O3 extra-heavy-combine/heavy-delay interpreter fixture passes the same 50
validation checks and 1,538 executions in each of byref and merged scalar rings
of sizes 1 and 2. Values include every byte-pattern payload/padding, non-null
references with zero payload and null references with nonzero payload.
The r172 binary oracle passes all 25 exact valid/invalid fixtures against Wasmtime.

The final r176 actual CLI runs pass 16 cases across 26 ordinary configurations
(416 runs) and 4 ROS configurations (64 runs), including a 20,000-call hot loop.
These configurations cover full/lazy/lazy+verification and tiered variants where
available, instruction/unwind, and standalone validation. Both products also pass
independent feature gates/conflicts and cold/warm/disabled signed-cache checks.
Wasmtime v48.0.2 passes all 16 cases, including the new hot-loop case.

Actual signed-cache objects pass assembly inspection for 6 functions in each
frame policy per product: a direct x86-64 tag test, no success-path calls or spills
in unwind mode, two frame-maintenance calls in instruction mode, and a separate
cold null-trap call. Both retain .eh_frame. Six actual interpreter handlers per
product have 5-instruction success paths without calls/spills; the four tail
variants end in mandatory indirect jumps. These are reference-path checks, not a
claim of complete VM performance qualification. Memory-access lowering is unchanged.

A generic vector-lane tag extraction experiment (r174) passed semantics but made
ARM32/i386 emit SIMD loads/extracts; it was rejected. The retained transform is
specific to the measured x86-64 layout. r175's 12 non-x86-64 assembly files are
byte-identical to the previous scalar implementation; all six QEMU targets pass
2,304 tag/payload/padding cases per product. r176 additionally resolves an absent
native fragment triple using LLVM's native triple, as the surrounding emitters do;
its actual native object inspection confirms the transform is applied. Explicit
cross-target triples retain their r175 behavior.

The first r176 fixture links failed when /dev/shm reached its 8 GiB capacity;
logs are retained as `test.build.disk-full.log`. Runtime objects were already
complete. After hash-verified archival of older artifacts, both fixture commands
were rerun and passed with identical before/after source fingerprints. Builds,
execution and assembly inspection stayed in SSH Linux's 64 GiB/no-swap cgroup on
CPUs 0,2,4,6,16-31; no local build or test was run.

Old r79–r81 artifacts were archived and all 1,486 regular files verified before
remote pruning: `archive-tmpfs-artifacts-r79-r81.tar.gz`, SHA-256
`df2d8ea2a239537614c8e792a5ac98bbd8fdf4d915053cf1f4b6ca247e62cf13`.
The r170–r172 and r173–r174 source snapshots were separately archived and pruned
with per-file manifests. Do not use those pruned directories as build bases.

## Null-reference branches (r177–r179)

`br_on_null` and `br_on_non_null` now execute behind `function-references` in
both products, including standalone wasm3 and inline validation. The ordinary
lazy scanner decodes their complete u32 label immediate before advancing. Pointer
updates carry checked-range comments. The label prefix is reified in polymorphic
code; a missing/unknown reference refines to reference-only bottom, never i32.
An empty/numeric label for br_on_non_null is invalid even in unreachable code.
The normative rules are [branch validation](https://webassembly.github.io/spec/core/valid/instructions.html#valid-br-on-null)
and [branch execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-br-on-null).
Rich heap declarations remain outside the integrated frontend.

The interpreter tests the reference kind in its existing operand slot and retains
numeric register-ring state. Only a null value is removed: on the taken edge of
br_on_null, or on the fallthrough edge of br_on_non_null. Taken edges share the
existing tuple-preserving stack repair and loop register transformation. r179
bypasses a repair thunk when it emits no operations and needs no loop transform.
LLVM passes the original SSA reference to the destination PHIs; the tag test emits
no runtime helper or scratch reference copy.

Both r179 products pass 42 standalone/integrated validation checks and 4,096
executions per byref/ring-1/ring-2 interpreter configuration, compiled with O3,
extra-heavy combine and heavy delay. Each JIT frame policy passes the 42 checks
and 4,096 executions. All 21 exact binary validation fixtures agree with Wasmtime.
Twenty authored CLI cases cover null/non-null edges, nested and tuple labels,
20,000-iteration loop parameters, unreachable refinement and invalid inputs;
Wasmtime passes all 20. Actual ordinary full/lazy/tiered/validation configurations
pass 520 runs, ROS passes 80, plus independent feature and signed-cache checks.

The r179 actual-artifact audit passes in each product: 8 functions per JIT frame
policy have a direct tag test; unwind has zero calls and frame spills, instruction
has only its two frame-recording calls. .eh_frame remains present. Twelve actual
interpreter handlers contain a single tag read and no calls/spills; the eight
mandatory-tail variants occupy at most 22 bytes and end both edges in indirect
jumps. These are focused code-generation checks, not final platform/performance
acceptance. The supplemental standalone heavy-combine/heavy-delay driver passes 11 control-flow
fixtures in each of byref/ring-1/ring-2, per product, including 20,000-iteration
null/non-null loops and multi-value labels. Its initial failures were test-adapter
issues (a void-function assertion macro, missing call hook and the call-info
pointer encoding); the final adapter resolves owned call records and runs complete
callee bytecode. Actual VM call behavior is independently covered by the CLI runs.
QEMU results are recorded separately when complete.

r172–r176 artifacts are archived in `reference-execution-r172-r176.tar.gz`,
SHA-256 `5bccb70ae656ddcb600565cc1484cbb0c7a9ba11a2c0dfe2c1903e849360a780`.
All 2,451 regular files were verified against their manifest before remote pruning.
The source snapshots r175/r176 remain intact; source pruning and artifact pruning
are distinct operations. Current builds use r179, whose source fingerprints are
recorded with each artifact.

### r179 target execution and native objects

Both products' AArch64 and big-endian s390x Clang/LLVM/QEMU interpreter profiles
pass 42 standalone/integrated validation checks and 4,096 executions in each of
byref and register rings 1/2. LLVM machine verification runs with musttail intact;
the original target IR and assembly are retained compressed with the binaries.
These are complete parser/initializer/interpreter fixtures, not complete target
JIT coverage.

Separately, both actual AArch64 full-JIT runtimes pass `ref.as_non_null` (50
validation checks, 1,538 executions) and reference branches (42 checks, 4,096
executions), under each of instruction and native-unwind policies. This cross
profile uses the concurrent allocator and the hash-verified bundled LLVM
23.1.1-uwvm-ros.7 target library. Null traps check real call-stack diagnostics.
Twenty-eight functions per product are inspected from actual runtime-generated
cache objects: instruction recording adds two calls, unwind removes both, branches
have no helper calls, and null-check functions retain only their semantic trap
call. Every object retains .eh_frame.

The first AArch64 runs passed execution but failed cache-directory creation:
QEMU's `-L` rewrote the absolute root directory handle into the full `/work`
sysroot. The test driver now uses a relative private cache path. All eight
suite/product/policy combinations were rerun successfully, captured their target
objects, and passed native call-count inspection. Initial warning logs are kept;
only the subsequent relative-cache run supports the object-inspection claim.
This is still focused AArch64 full-JIT qualification, not the final QEMU matrix,
Wasm exceptions, debugger, hot replacement, or global performance acceptance.

The r177–r179 build/execution/codegen evidence is archived as
`reference-branches-r177-r179.tar.gz`, SHA-256
`2b66f7fe301f4076d03f00f634810553fec647df0a2e5e4a74e417f7c9862f33`.
All 1,970 regular files were verified against their per-file manifest. Failed
incremental builds and test-harness attempts are retained alongside passing runs.
These recent remote artifacts remain available; only the separately archived
r172–r176 artifacts were pruned.

## Owned Core 3 function signatures (r182)

The actual parser now decodes explicit nullable `(ref null func)` / `(ref null extern)`
function parameter/result encodings with the shared bounded Core 3 reader when
function-references is enabled. Signatures retain owned `core_value_type` vectors
and a separate lossless legacy carrier projection. Section copying rebinds views
to the copy; moves preserve inner allocations. Borrowed legacy parsing remains the
disabled-feature path. Non-null, concrete heap, and GC signatures still fail with
an explicit not-integrated diagnostic: this does not complete typed declarations.
The fundamental recursive/owned-signature module units are independent of the
full parser to avoid a named-module and textual-header dependency cycle.

Both products pass 1,224 ASan/UBSan boundary/policy/ownership checks, including
1024 owner relocations, source destruction, deep copying and moving. Eight binary
fixtures explicitly replace shorthand reference signature bytes with `0x63 heap`;
Wasmtime v48.0.2 passes all eight. Actual VM execution passes 208 runs across 26
ordinary configurations and 32 across four ROS configurations, including feature
rejection and cold/warm/disabled JIT cache checks.

Each product additionally passes 50 integrated validation checks and 1,538
reference-payload/null-trap executions per interpreter configuration (byref and
merged scalar register rings 1/2, O3 extra-heavy combine/heavy delay) and per full
JIT stack policy. The execution signatures use the new binary encoding. Actual
assembly checks cover 12 JIT functions and six interpreter handlers per product:
four musttail paths retain indirect jumps with no hot helper calls/spills, and
unwind removes instruction-stack maintenance calls.

For each product, eight pairs of real cached JIT objects (reference, tuple,
indirect call and linear-memory fixtures; instruction/unwind) have identical
machine-code bytes and identical relocations after replacing only the necessary
module-content hash in symbols. This supports no added execution cost for the
new signature encoding in these cases. It is not a parser-throughput benchmark,
a full combine/delay matrix, cross-target acceptance, or global performance proof.
Both module import/export audits pass; wasm1p1/wasm2 pure validator directories
remain HEAD-clean. Initial failed dependency/narrowing builds are retained.

## Concrete function heaps and initialization policy (r183–r185)

`ref.null <typeidx>` now uses the bounded signed-33 heap decoder in standalone
Core 3 validation, integrated interpreter/JIT compilation, ordinary lazy scanning,
actual JIT emission, and global/table/element initialization. The complete heap
code is retained in initializer IR. This adapter explicitly requires the current
function-only type table: it is not a mixed GC-type classifier. Other heap
hierarchies and non-null/concrete declaration types remain unintegrated.

Function-reference policy is checked independently during parsing, validation and
initialization. Parsing with the feature enabled and instantiating with it disabled
rejects explicit signatures and concrete `ref.null` initializer expressions. The
signature parser ADL hook is constrained to the extended feature pack and normal
function type representation; Wasm 1.0-only users retain their original path.

Final r185 checks pass in both products: 1,229 signature ownership/policy checks,
56 initialization-policy checks, and the unchanged heap decoder's 2,382 r183
ASan/UBSan boundary checks. Eighteen new heap-immediate CLI cases agree with
Wasmtime v48.0.2. Ordinary mode coverage passes 468 runs across 26 configurations;
ROS passes 72 across four configurations, with feature rejection and cold/warm/
disabled-cache checks. Final-source explicit-signature regression also passes
208 ordinary and 32 ROS runs. Disabled Core 2 routing intentionally retains its
legacy invalid-immediate diagnostics; neither legacy pure validator was changed.

Each product passes 40 pure/integrated heap validation cases and 261 executions
in O3 extra-heavy-combine/heavy-delay byref and register rings 1/2, and separately
in each full-JIT instruction/unwind policy. Actual signed JIT objects contain no
semantic helper calls for these null constructors: instruction recording adds two
calls per function and unwind removes both. Twelve functions and six interpreter
handlers per product were inspected; four tail handlers remain indirect jumps
without calls, checks or spills. The objects retain .eh_frame. Eight additional
short-versus-concrete-heap module pairs per product have identical actual machine
bytes and relocations after normalizing only module-content hashes, including
linear-memory cases. These are focused execution-cost checks, not whole-VM
performance acceptance or complete cross-target qualification.

Evidence through r184 is archived in `core3-signatures-and-heap-r180-r184.tar.gz`,
SHA-256 `de2ec009bfaf72c8b9b9cbe9e6d3dccf1aae13b749e22e11912c7cc5dde36d45`.
All 837 regular files were verified before the superseded remote artifact files
were pruned. r185 evidence remains available. r180–r182 source content is archived
separately in `archive-tmpfs-sources-r180-r182.tar.gz`, SHA-256
`d5fe7ca51dc993495fa30e757dc60ebbcd0b26fe97de6d830c180db42dffea10`; only r180/r181
source content was pruned. The eight old r179 QEMU trap core files were separately
verified/archived in `qemu-null-traps-r179-cores.tar.gz`, SHA-256
`b687cd7e72b0ac231193eae0f59838f0d50a55ad0bc29c1d87c99fbefdc692ab`.
The previously archived r177/r178 artifacts have also been pruned after per-file
verification; their source snapshots and r179 artifacts remain available.

## Explicit block and typed-select value encodings (r186)

The bounded Core 3 value reader now accepts explicit nullable abstract function/
external references at block/loop/if result-type and typed-select value-type sites.
It retains the complete decoded type and only projects lossless existing carriers;
non-null, concrete and GC declarations still fail closed. Pure validation, both
integrated compilers, ordinary lazy scanning and the actual JIT block resolver use
the shared decoder. SIMD/reference-types gates remain independent. Parser/control
cursor commits and the existing null-reference interpreter handlers have explicit
memory-safety annotations. Guest reference/frame layouts are unchanged.

Each product passes 10,818 ASan/UBSan value-decoder checks and 48 pure/integrated
validation cases. Byref and register rings 1/2 at O3 extra-heavy combine/heavy delay
each pass 5,120 full-payload/reference-null/branch/select executions. The full JIT
passes the same executions under each instruction and native-unwind policy.
Sixteen new binary CLI cases match Wasmtime v48.0.2; ordinary configurations pass
416 runs and ROS passes 64, including feature gates and cold/warm/disabled caches.
Both module import/export audits pass, and the wasm1p1/wasm2 pure validator
folders remain HEAD-clean.

For each product, 16 pairs of actual cached JIT objects have identical machine
bytes and relocations for short and explicit encodings, including block/loop/if,
typed select, tuple, indirect-call and memory examples. Sixteen actual fixture
functions per product retain .eh_frame: instruction recording contributes two
calls per function and native unwind contributes zero. This verifies focused
code-generation equivalence, not whole-VM performance or final QEMU acceptance.

The completed r185 artifacts are archived as
`core3-ref-null-and-signatures-r185.tar.gz`, SHA-256
`a6daabd69d17be87c98341af0cb180d58a55042bd07dc92f3dfb08d82259325c`.
All 2,026 regular files were verified before remote pruning. r183–r185 source
content is archived in `archive-tmpfs-sources-r183-r185.tar.gz`, SHA-256
`84b133fe68c144838ded1fecfca9bdc3d61dd52acc5fd585243fc836ec2143d4`;
only superseded r183/r184 source content was pruned. r185/r186 source snapshots
and r186 execution/codegen artifacts remain available.

## Explicit local declarations (r187)

The actual code-section parser accepts explicit nullable abstract function/external
local types through the shared Core 3 reader, while preserving Wasm 1.0 and custom
one-byte parser hooks. Zero-count declarations still validate their encoded type.
The code section retains the encoding's function-reference requirement, and the
initializer checks it when a caller supplies a stricter policy. Initialization
also checks compressed local runs for independently disabled reference/SIMD types.
Non-null, concrete and GC locals remain explicitly rejected until rich validation
and execution are integrated.

Both products pass 235 ASan/UBSan parser boundary/policy/owned-local checks and
268 initializer policy checks. The execution fixtures pass 5,120 default-null,
local.set/tee, branch/select, full reference-payload and register-ring cases in
each O3 byref/ring1/ring2 configuration and each full-JIT stack policy. Their 48
standalone/integrated instruction-validation checks are the r186 block/select
regression, not additional new local-declaration cases. Eighteen actual local-type
binary CLI cases match Wasmtime v48.0.2; ordinary modes pass 468 runs and ROS 72,
with feature gates and cache cold/warm/disabled checks. Module import/export audits
pass and the legacy pure validator folders remain unchanged.

Each product's 16 short/explicit-local pairs have identical actual JIT machine
bytes and relocations. Sixteen native fixture functions per product retain
.eh_frame and remove both instruction-stack calls under native unwind. These
results cover current encoding paths, not rich local definite-initialization,
whole-VM performance, or final target-matrix acceptance. A further independent
compiler-policy check is being added for callers that change validation options
after parsing and initialization; these r187 CLI checks alone do not establish it.

Completed r186 artifacts are archived as `core3-block-select-values-r186.tar.gz`,
SHA-256 `5cd74bae21c8b6dc19fbc3dc659008db86c0c86b5d82c468243e89fdabe99f35`.
All 1,186 regular files were verified before remote pruning. Source snapshots and
r187 execution artifacts remain available.

## Declaration policy enforcement and external encodings (r188–r192)

The compiler independently checks declaration requirements after a caller parses
and initializes a module with a more permissive policy. These checks run in the
pure wasm3 validator, its legacy facade, integrated interpreter/JIT validation,
and ordinary structured/function-only lazy splitting, before publishing execution
units. A type-section aggregate avoids rescanning all signatures per function;
compressed local runs retain their encoding requirement even when their count is
zero. The compatibility multi-result disable flag is also honored. The local
record remains eight bytes on the qualified x86-64 host.

r190 passes 1,234 owned-signature checks and 72 independent declaration-policy
cases per product, including ASan/UBSan interpreter validation and actual JIT
compilation. Signature/local CLI suites pass 208/468 ordinary runs and 32/72 ROS
runs, with feature gates and cold/warm/disabled cache checks. Each product's 24
short/explicit pairs produce identical actual JIT text and relocations, including
memory access. These are scoped code-generation checks, not whole-VM throughput
or final-platform acceptance.

r191 adds explicit nullable abstract function/external types to table/global
definitions and imports. r192 adds them to active, passive and declarative element
segments, including empty expression vectors. Each section retains its binary
feature requirement through copying, initialization and independent compilation;
the metadata does not alter guest reference values or generated memory guards.
The bounded value decoder is shared with locals. Non-null, concrete and GC value
declarations still fail explicitly pending rich type-system integration.

Both products pass 337 external-declaration parser checks, 681 element-declaration
parser checks, 668 initializer-policy checks and 168 pure/facade/integrated compiler
policy cases (ordinary lazy barriers included). Parser, initializer and interpreter
policy checks use ASan/UBSan. Actual JIT compilation passes the same 168 cases.
Twenty-four external/import cases and 22 element cases match Wasmtime v48.0.2.
The final r192 CLI passes 624 external and 572 element runs across 26 ordinary
configurations, and 96/88 runs across four ROS configurations. The element cases
cover table.init, table access and indirect calls. Each product's 16 short/explicit
element pairs have identical actual JIT machine code and relocations; r191's 16
external-declaration pairs also match. Both module import/export audits pass;
legacy wasm1p1/wasm2 pure validator directories remain unchanged.

Qualified r192 source fingerprints: ordinary
`03d4b53c18705baab5f2bf7a8b2c8d123e07714e38092fd9f837af6a00dd21ba`, ROS
`1595c9f59a766b3ea3adf8639962dd1b080df111e37ecda1966d5d6284698d5a`.
The r187/r188 evidence archive is `core3-local-and-policy-r187-r188.tar.gz`,
SHA-256 `c5bbe2cdfe5fdd7b4f539acd558b5bc371985cec0cf577d8f2e85ab6e66cbe06`.
The r189/r190 archive is `core3-declaration-policy-r189-r190.tar.gz`, SHA-256
`aa3f279f488a469a7c5f6a6313b8925e48363689dd0c20fed3921a3869f2812f`.
Every archived regular file was digest-verified before remote pruning. Remaining
r179 artifacts were likewise verified against `reference-branches-r177-r179`
before pruning; prior AArch64 and big-endian evidence remains in that archive.

## Function reference lowering (r193–r194)

Full-JIT ref.func now constructs references in SSA using relocatable, module-owned
function records. Distinct import aliases retain their original identities. The
cache ABI distinguishes this representation from older bridge-based objects.
Known tag bits eliminate redundant ref.as_non_null trap blocks without using
payload or padding as a nullness test. This compile-time proof does not add guest
instructions or memory-access guards.

Both products pass 3,072 native reference identities in each stack policy,
50 null-assertion validation cases and 1,538 null/identity executions per policy.
Actual cold/warm cross-process JIT objects pass cache isolation/rebinding; each
of three ref.func/ref.as_non_null getters has two instruction-frame calls or zero
calls in native-unwind mode. The initial r193 objects exposed a redundant trap
branch even with pb-o3; r194 removes it. Both products' constructor lowering passes
six QEMU profiles (i386, AArch64, RISC-V64, ARMHF, PPC32 big endian, SystemZ) with
512 unaligned/canary cases per profile. r194 ordinary dynamic tag tests pass
2,304 payload/padding/tag cases on each profile. Actual AArch64 full-JIT r193
identity execution and getter assembly pass both products and both policies;
that earlier target run does not qualify all of r194's later null-folding changes.
These are focused checks, not the outstanding full-platform acceptance matrix.

## Core 3 tag declarations and identities (r195–r199)

The parser registers section 13 in the normative position between memory and
global sections. Local tags and kind-4 imports/exports check the literal zero
attribute, bounded u32 type indices, empty function results, index-space bounds,
section duplication/order and resource limits. Owned indices survive parser
copies. `--wasm-feature-enable-exceptions` / `--wasm-feature-disable-exceptions`
control the declarations independently, including an empty tag section. Parser,
initializer and pure/integrated/lazy compiler policy barriers enforce the gate.
Pure validator edits remain confined to wasm3.

Initialization allocates distinct local tag identities before linking. Imports
and transitive reexports resolve to the original instance; structural signature
equality never merges distinct local tags. Linking rejects payload type mismatch,
cycles and unresolved/wrong-kind targets before guest publication. Rich recursive
payload types and native host tag-provider APIs are not integrated yet. No
try_table, throw, throw_ref or exception-reference execution is claimed here.

The r198 source passes 265 ASan/UBSan parser checks, 715 initializer policy checks,
217 pure/facade/integrated policy cases (both ASan interpreter and actual JIT),
and 487 ASan tag-instance/alias/rejection checks per product. Twenty-one tag
cases match Wasmtime v48.0.2. CLI runs pass 546 ordinary / 84 ROS cases, feature
gates, section-detail rendering and cold/warm/disabled cache checks. Each product
passes four actual JIT text/relocation comparisons for memory accesses/loops with
versus without tag declarations, under instruction/unwind policies. Both module
import/export audits pass. These scoped checks do not qualify guest exceptions or
the full QEMU matrix. r199 additionally fixes Core 3 default policy selection,
marks tag payload types as used in warnings and exposes tag resource limits;
both r199 runtime builds pass 217 declaration-policy cases, parser/initializer ASan checks (265/715), and 546 ordinary / 84 ROS tag CLI runs. Each product additionally passes 12 tag resource-limit boundaries and 2 tag-only type warning checks. Module audits pass.

r198 source IDs: ordinary
`49623a434b411e73f562c6acfd10161a13f8f2038202405470c2eae4679e72cf`, ROS
`d367ca3e07fa5352ea84177c7d5f2c5fcb40cd4da38cc48ac6bc74c59d6618d3`.
The r192 archive `core3-element-declarations-r192.tar.gz` has SHA-256
`1fad946621424aa381c70139d7ec5f955405ea72939a0b062f04cc543c0da682`.
The r193 archive `core3-ref-func-pre-fold-r193.tar.gz` has SHA-256
`82e1c6efb86914480d87c14d40675d7b7bcd5c3a148fe402277bfb5e5875d6c2`.
All regular files were digest-verified before remote artifact pruning. r193's
initial redundant-branch diagnostics remain in its archive.

## Core 3 exception instructions (r200–r204)

The transactional catch-vector decoder recognizes all four current Core 3 clause
forms. Catch kinds are literal bytes, tag/label indices are bounded u32 LEBs, and
counts are bounded against the actual remaining bytes before allocation. Failed
input preserves the caller cursor and publishes no partial handler vector. Both
r200 ASan/UBSan runs pass 5,347 protected-page, truncation, padded-LEB and overflow
checks. New helpers have matching .h/.cppm exports in the wasm3 directory.

Zero-handler `try_table` now uses existing block lowering in pure validation,
uwvm-int full/lazy and LLVM full/lazy/tiered (ROS full only). Twenty-one new-grammar
cases agree with Wasmtime v48.0.2; 546 ordinary and 84 ROS CLI runs pass across the
supported mode/trace combinations, including independent scoped feature gates.
The untouched complete legacy selector reports an unknown opcode; explicit scoped
disabling reports the exceptions feature. Four actual JIT text/relocation pairs
per product are identical to equivalent `block` memory/loop programs under both
instruction and unwind policies. In heavy combine+delay builds, uncached and
one-/two-slot register-ring compilation produce byte-identical threaded code
(72/80/80 bytes) for the paired body, with 42 executions per product. Disassembly
checks of 24 i32 add/combine tail functions per product find indirect jumps and no
calls/returns. These are scoped code-generation checks, not a full performance matrix.

Shared rich-type rules validate all four catch payload signatures, non-null exn
appending, covariant reference matching, throw payload order, nullable throw_ref
input and stack polymorphism (538 ASan/UBSan checks per r201 product). They are not
an exception runtime. The pure wasm3 validator integrates tag payload lookup,
outer-context catch labels, loop-parameter labels and throw stack effects for its
currently admitted carrier types. r204 ASan builds pass 45 current-grammar and
malformed-binary cases (180 comparisons per product) against Wasmtime, explicit
policy, the Core 3 default overload and disabled exceptions. Imported function/tag
indices, duplicated catch tags and catch-all-before-tag are included. Concrete exn
and rich declarations still need the pending main type-system integration.

Runtime revalidation now retains tag sections/imports and declaration encoding
policy when rebuilding a parser-shaped module from instance storage. It reserves
all five import kinds before publishing descriptor pointers. The expanded
217-case declaration-policy fixture also tests this rebuild and the default
Core 3 overload. Both r204 ASan/UBSan interpreter runs and JIT fixtures pass. The
unchanged runtime objects were qualified with corrected test harnesses, retaining
the original failed build logs and explicit fixture-qualification manifests. Final
r204 CLIs pass 546 ordinary / 84 ROS zero-handler cases and the same counts for
tag declarations. Each CLI also passes all 45 pure exception cases; all four JIT
code/relocation comparisons per product and module dependency audits pass.

At r204, nonempty handler tables and throw execution were still rejected by the
backends; the later r205–r207 same-function execution slice is described below.
Acceptance by pure validation alone is not execution support. Native cross-function
exception dispatch, retained references and unwind/instruction cleanup remain to be connected.
The r202 integration compile diagnostics, r203 polymorphic-stack failure, and
corrected test-harness failures are retained; they are not passing evidence.

r204 source IDs: ordinary
`7d2ab0aac718c29aa6b6b1d218e9a227cf92996e53c858293ccd3e9cdd95792d`, ROS
`3b160b582d54581c447fff4d64bb342aa0a0ca9db4f2312f5398af59af5d3126`.
The `core3-tag-integration-r195-r198.tar.gz` archive has SHA-256
`f0a96df84985b2078df1893605c6c3c19c39a3cb40d0d1b49a37a8da464b8e94`.
The old-source archive `archive-tmpfs-sources-r192-r199.tar.gz` has SHA-256
`b935168950a08858cf01eb06af4df1e0dfd8757f7bc00e9d19bdc417bb8f99d1`.
All regular files were verified before remote pruning. A failed r202 staging copy
caused by tmpfs exhaustion was discarded and recreated after verified archival.

The final artifact archive `core3-exception-control-r199-r204.tar.gz` has SHA-256
`bf0ee856d3cc36161a4d77cee2520ded95a6f8e27b197e87c7d95067e872db2b`.
All 5,395 regular files were verified; only superseded r199–r203 artifact files
were pruned. The qualified r204 runtime objects, binaries and logs were also
preserved in the later r204–r205 artifact archive listed below.

## Same-function exception execution and evidence correction (r205–r207)

Both backends now lower a lexically caught `throw` to the existing tuple branch.
Nonempty `try_table` supports `catch` and `catch_all` for the admitted flat payload
carriers. The compiler resolves actual tag instance identity, checks outer labels
and preserves catch order; handlers are compile-time metadata, with no runtime
handler-stack updates. Nested handlers, same-signature distinct tags, imported
aliases, loop parameter labels, payload discard, and normal exits use existing
control-flow/operand-stack machinery. The interpreter repairs its register ring
through existing branch lowering; LLVM uses the existing destination PHIs.
Escaping throws, cross-function exception propagation, `catch_ref`, `catch_all_ref`
and `throw_ref` execution remain unsupported. This optimization is not a native
exception runtime, and traps still bypass guest catch handlers.

The ordinary lazy structural scanner consumes bounded catch vectors and throw
indices so the supported same-function slice works across the requested ordinary
execution modes; ROS retains its full interpreter/full JIT scope. The separate
JIT runtime-validation module rebuild now carries tag declarations/imports and
reserves all five import kinds before publishing descriptor pointers. The r207
fixture passes 217 declaration-policy cases and 72 JIT rebuilt-module tag/throw/
try_table policy cases per product. These are compilation/validation checks, not
72 cross-function EH executions.

r205 heavy combine/delay interpreter builds pass 42 actual executions per
product across uncached and one-/two-slot ring configurations, including modular
arithmetic edge inputs, nested wrong-tag handlers and catch-all payload discard.
The generated assembly checks cover 24 arithmetic/combine tail functions plus
five exception-branch handlers per product; required dispatch remains an indirect
tail jump. r206 ASan/UBSan runs pass the same 42 interpreter executions per product.
These checks do not claim the complete combine/delay configuration matrix.

r207 CLI qualification contains 33 current exception-syntax cases. Ordinary
qualification passes 858 UWVM runs plus 33 Wasmtime runs (891 total); ROS passes
132 UWVM runs plus 33 Wasmtime runs (165 total). Coverage includes scalar/tuple/
float-bit/vector/reference payloads, nested/ordered handlers, outer labels,
loops, side effects, malformed payloads/tags/labels, and traps which catch-all
must not catch. Independent feature disabling is also checked. Imported-tag
alias/cache qualification passes 30 ordinary and 10 ROS runs.

r207 production source IDs: ordinary
`cb3c16c9935742a5ecc67b151a39d82c43a71dd3496ca22eb6dbea6441405732`, ROS
`03f47b46a00e38991e36b14839cbafce57678e8b7a2e3118f5c726667d0a6715`.
The later CallBase and resource-cleanup preparation belongs to the separate
r208 qualification below, not those r207 results.

**Code-generation evidence correction:** eight comparison scripts previously
extracted only `.text`. Some actual large-code-model objects instead contain an
empty `.text` and nonempty `.ltext`, so their former `text_bytes=0` comparisons
did not establish machine-code equality. Those checks are superseded by a new
bounded ELF reader which covers ELF32/ELF64, both byte orders, and every section
with `SHF_EXECINSTR`, regardless of its name. It preserves section boundaries,
order, names, types, flags, alignment and actual bytes, and rejects objects whose
total executable byte count is zero. Only `uwvm_m_<hex>_` names are normalized;
complete object relocations are still compared without removing kinds, offsets,
addends or function indices.

The correction applies to `check_exception_local_codegen.py`,
`check_try_table_codegen.py`, `check_tag_declaration_codegen.py`,
`check_function_signature_codegen.py`, `check_element_value_codegen.py`,
`check_external_value_codegen.py`, `check_local_value_codegen.py`, and
`check_value_immediate_codegen.py`, identically in both repositories. Nine
regression-test groups pass in the SSH Linux cgroup, including empty `.text`
with nonempty function sections/`.ltext`, differing machine bytes, differing
section splits with identical concatenated bytes, malformed boundaries, and
relocation changes.

The preserved historical objects were re-read without rebuilding or rerunning
the JIT: **232 historical pairs plus all eight current r207 pairs pass** full
executable-section and complete-relocation comparison, with a nonzero code extent
for every object. Historical coverage spans r182, r185, r186, r187, r190, r191,
r192, r198, r200 and r204. The current r207 memory and memory-loop pairs contain
162 executable bytes under instruction tracing and 130 under native-unwind
tracing, per product. Their complete machine code equals the equivalent branch
program. This establishes these scoped code-generation identities; it is not a
claim of complete performance or platform acceptance.

The requalification archive `executable-section-reaudit-r208.tar.gz` has SHA-256
`2e57c6452d5293f5f1a65d47daa6d6a5e305e321b9853e423eb95f3b11f18e5d`.
All 2,887 recorded files (3,891,564 bytes) were checked against their individual
hashes after transfer. It retains the old/current objects, full relocations,
per-section bytes/manifests, reader/test sources and qualification reports. The
input bundle `executable-section-reaudit-inputs.tar.gz` has SHA-256
`9fd298daaeaed0fed0aa8c3ad6a448378b6521a8ba198873248853579fe6d9b6`.

The earlier `core3-exception-local-r204-r205.tar.gz` archive has SHA-256
`68735217714fbc8708882f99c3c44289361271fedb92564cc3c7f5437154d49b`
and records 3,257 files (490,227,834 bytes). The source archive
`archive-tmpfs-sources-r204-r205.tar.gz` has SHA-256
`b37cb46d5d958baa5d734e3b282a0df2a3ed7c96aa24dc3099f62796c6dec175`
and records 11,855 files (150,211,574 bytes). Earlier archive SHA entries above
remain valid; this requalification replaces only the incomplete `.text`-only
machine-code comparison evidence.

## Activation cleanup and current exception qualification (r208)

The frozen r208 builds retain the same-function exception execution scope above.
Runtime preparation makes each interpreter scratch mark release through RAII on
normal return or a tail-transfer return. The ordinary tiered native-entry thunk
also restores its borrowed boundary pointer before the stack record dies. These
changes prepare exceptional cleanup without changing the existing `noexcept`
entry interfaces, enabling cross-function guest exceptions, adding logical JIT
frames under native unwind, or adding memory-access guards/locks.

Both runtime objects and CLIs were rebuilt from the frozen r208 sources inside
the SSH Linux cgroup: 64 GiB, zero swap, CPUs `0,2,4,6,16-31`. Production source
fingerprints agree before compilation, after compilation and at qualification.
Each product passes the existing 217 declaration-policy cases plus 72 checks of
JIT rebuilt-module tag/throw/try_table policy and imported-descriptor ownership.

The current exception CLI suite contains 37 syntax cases. Ordinary qualification
passes **962 UWVM runs plus 37 Wasmtime runs (999 total)**; ROS passes **148 UWVM
runs plus 37 Wasmtime runs (185 total)**. UWVM totals include pure validation as
well as the requested ordinary full/lazy/tiered execution modes or ROS full
interpreter/full JIT modes. Independent exceptions feature disabling passes.
These scoped tests include locally caught exceptions and invalid programs;
they do not establish cross-function EH or retained exception-reference support.
Tag-alias topology with cold/warm object caching passes 30 ordinary and 10 ROS
runs.

The corrected executable-section reader qualifies four actual memory/memory-loop
JIT code comparisons per product. All nonempty executable sections and complete
relocations match their equivalent branch programs. Each instruction-policy
object has 162 executable bytes and each native-unwind object has 130 bytes.
The old empty-`.text` evidence is not reused.

The production activation-cleanup helpers pass 2,750 ASan/UBSan checks per
product using normal returns, early returns, simulated throws, 64 nested records,
and repeated inner catches while the outer activation remains live. Tests verify
scratch mark/release counts and restoration of borrowed atomic pointers before
record destruction. They exercise the shared cleanup components with throwing
test callbacks outside the runtime's unchanged `noexcept` entry ABI. Dedicated
Clang `-O3` manual/RAII probes have byte-identical machine code: 32 bytes for
scratch cleanup and 26 bytes for boundary publication/restoration, per product.
Both module dependency and pragma audits pass, together with 45 checker semantics
tests and nine ELF-reader regression groups. The cgroup's historical OOM counters
did not increase during this qualification.

r208 production source IDs: ordinary
`773a77fec21dd6b837b82cc3bd2f872a8881119b7c4ed89901157a1c71cbbf44`, ROS
`95952810dbe12458814af0cd9ec48da05083722dcf6f2b3aeb4ea26103894e90`.
The corresponding CLI SHA-256 values are ordinary
`aac3cd1fd9fe7fd0f5aee85a7bbbc57a7c59e33dfb200fa01a4aa936bc1a2e38` and ROS
`eebd73afc5227fe89e54bda13c2f452a76d1d64381a45d6fe72b5513db17c3bf`.
Reports remain at
`/dev/shm/uwvm-builds/shared-r208/{uwvm2,uwvm2-ros}/qualification.json`;
all r208 source snapshots, runtime objects, CLIs, commands and test artifacts are
retained as the next development baseline.

Superseded r206/r207 artifacts are preserved in the ordinary repository's
`build/wasm3-evidence/core3-exception-local-r206-r207.tar.gz` with SHA-256
`6184ca0094c030b68f81afc485b6d11c2b7ec209f361a53c1b418b732acc1515`: 2,106 regular files,
802,089,183 uncompressed bytes. Their source snapshots are preserved separately
in `build/wasm3-evidence/archive-tmpfs-sources-r206-r207.tar.gz` with SHA-256
`2d6d34bc8edce4664218b50cf8e144fc089f9eb35dfc4feb1dced4505a47174c`: 11,874 regular files,
150,335,668 uncompressed bytes. Per-file manifests and archive verification
reports accompany both archives. Every archived regular file was verified after
transfer and checked again against the remote file before pruning. Only those
verified r206/r207 files were removed; dependency symlinks and every r208 file
remain. This released tmpfs capacity for the next implementation batch.

## Native exception foundations (focused overlays before r209)

The following are independently qualified building blocks. They have not been
connected to complete cross-function Wasm exception execution through the CLI.
The supported guest execution slice remains the same-function `catch`/`catch_all`
behavior described above; retained `exnref`, `throw_ref`, and the complete Core 3
reference/GC integration still require implementation.

`runtime/exception/value.h` separates an immutable exception instance from each
native throw activation. It preserves numeric/vector payload bits, retains
explicit roots for reference payloads and owns a tag-identity token. Each C++
throw receives an independent ABI exception header; repeated or overlapping
throws never reuse a TLS singleton unwind record. Moved-from reference fields
retain native null bits, and moving a guest activation preserves a valid immutable
value reference in the still-accessible source. Root tokens provide an ownership
interface for future collector integration, not a completed cyclic Wasm GC.

Both products pass 1,535 ASan/UBSan ownership checks with leak detection, including
retained rethrows and overlapping throws of the same immutable instance on four
threads. Header checks under `-fno-exceptions` pass while `throw_value` is correctly
unavailable. Real value/impl BMIs and consumers compile, link and run in both
exceptions and no-exceptions configurations. The new runtime/exception module
folder also passes the dependency checker. Complete evidence is archived in the
ordinary repository's
`build/wasm3-evidence/exception-value-ownership-r208-overlay.tar.gz`, SHA-256
`c58cc9bbea54e17b4655a8ea28bd1af52e1e7e4d060677f5643e07992aca2c4b`
(125 files, 44,190,368 uncompressed bytes).

The real parser/initializer now gives each local tag an independently owned
identity token; imported aliases retain their provider's token. A dedicated
ASan/UBSan fixture passes 12 checks per product: two aliases share identity,
same-signature local tags remain distinct, retained exception values survive
registry teardown/reinstantiation, and the final root release expires the old
identity. Failed overlay include-path setup attempts are preserved separately
from the successful third build. These checks cover initialized tag lifetime,
not exception dispatch or full VM shutdown qualification.

The LLVM symbol foundation uses genuine external typeinfo globals and a qualified
Itanium/DWARF personality, bound within each execution engine. It does not embed
host addresses into cached IR or register process-global replacement symbols.
The r7 symbol fixtures pass 53 native checks per product for typed catch, foreign
resume, independent bindings in simultaneously live engines and cached-object
relocation. The symbols module also passes real precompilation. Evidence is kept
in `build/wasm3-evidence/native-exception-symbols-r7/`; the SHA-256 of its complete
`SHA256SUMS` manifest is
`58e4f87283f7fca402c74e545d6210c5da9afb20ae7b30adf42259afe3b705d8`.
That qualification reports LLVM `23.1.1-uwvm-ros.7`.
Large symbol-probe artifacts in the retained r7/r8 directories are stored as
gzip files. Each `COMPRESSED.json` maps the original manifest entry and SHA-256
to its verified compressed file; small logs, sources and objects remain directly
available. The original manifest hashes above/below therefore remain the
identities of the verified uncompressed evidence.

The later r8 symbol qualification keeps the native MCJIT default code model and
also emits AArch64/RV64/i386 objects under both default and PIC settings. Its
112-file archive directory is `build/wasm3-evidence/native-exception-symbols-r8/`;
the complete manifest SHA-256 is
`ba8c0a5ad0abb90d98694ef8d5c6cc55d57fcfb5cc0c6546a4a3c92949556a7b`.
Focused QEMU symbol probes pass under the default model on AArch64 and i386.
RV64 passes only with explicit Large code model; the default model crashes on
the ordinary entry call because a distant host-call relocation truncates. That
failure remains a qualification limitation, not a passing default-RV64 result.
These are native symbol/ABI probes, not full Wasm runtime platform acceptance.
Complete QEMU r1-r3 evidence, including the failed default-RV64 attempt, is
archived in `build/wasm3-evidence/native-exception-symbols-qemu-r1-r3.tar.gz`,
SHA-256 `d4cecd73c355b540cafe31299ddcf1b9ad6cd1c7665b23f6fad33f391dcc4b48`
(112 regular files verified after transfer). The final symbols r4 matrix runs
both products on all three targets: each of the six profiles passes 53 checks
(318 total). Its 144-file verified archive is
`build/wasm3-evidence/native-exception-symbols-qemu-r4.tar.gz`, SHA-256
`522ee1d21424808aceadd9128bb424418a393b9995609ee2c210411b16300cb8`.

The product landingpad helpers pass 18,313 checks and 1,152 actual MCJIT executions
per product on Linux x86_64. Tests cover normal return, typed guest catch,
foreign exception resume, unmatched-tag `__cxa_rethrow`, and a caught helper
throwing a different guest or foreign exception. The latter resumes the new
landingpad record and balances the original catch; original/new object lifetimes
are checked. Each instruction-policy function exit performs exactly one pop;
native-unwind mode contains no logical trace symbols in either IR or the native
object. Cold and disk-backed warm object-cache runs retain identical IR, and the
first engine remains callable after constructing the second. Test-owned exception
classes isolate this emitter qualification from the unfinished Wasm value graph.
Symbols and landingpad modules plus an importing consumer pass real compilation.
These landingpad-r1 results report LLVM `23.1.1-uwvm-ros.6`; they are not silently
relabeled as the later symbols-r7 toolchain qualification.

The later QEMU landingpad matrix runs the same product helpers for both products
on AArch64 and i386 with the default model, and RV64 with explicit Large model.
Each of the six profiles passes 18,313 checks and 1,152 JIT executions, totaling
109,878 checks and 6,912 executions. Cold/warm caches, instruction/native-unwind
policies, rethrow and new-record cleanup all pass. The 198-file verified archive
is `build/wasm3-evidence/native-exception-landingpad-qemu-r1.tar.gz`, SHA-256
`51bde1649d2d549903da53204e2e9774943d74211b1143d5684743bb66c3214f`.
The earlier default-RV64 host-call failure still limits qualification; these
probe results do not change the product's default code model or enable Wasm EH.

Complete landingpad and initialized-tag-lifetime evidence is preserved in
`build/wasm3-evidence/native-exception-landingpad-tag-lifetime-r209-overlay.tar.gz`,
SHA-256 `d92721ee93e32a8f38049a8d45b38070f5c2a4507e4fb8fd329981b217a3cfee`
(122 files, 231,771,581 uncompressed bytes). This includes frozen overlay sources,
commands, successful/failed logs, native objects, full relocations, LLVM IR,
module artifacts and importing consumers. Every regular file was hashed after
transfer and checked again remotely before verified overlay files were pruned;
the r208 baseline remains intact.

The protected interpreter-call opfunc foundation passes 32 cases and 9,116 checks
per product in each of the optimized and ASan/UBSan builds. Coverage combines
direct/indirect calls, by-reference/uncached-tail/one-slot-ring/two-slot-ring
configurations, and normal/matching/nonmatching/foreign exception exits. Actual
x86_64 assembly has six tail instances without `ret`; matching-catch cleanup
reaches an indirect tail jump. The two by-reference instances return normally.
The scoped exception/value and optable module dependency checks also pass.
These opfuncs are not yet emitted by the validator. Reference tests establish
retained-root ownership, not the complete packed Wasm reference tuple ABI.
Evidence is archived in
`build/wasm3-evidence/exception-call-r208-overlay.tar.gz`, SHA-256
`7d076d6682c542917df24609c278c3c1c3ac17a0f2f0b28017c01cef6b08b830`
(76 files, 10,389,643 uncompressed bytes), with every regular file verified.

Emitter integration must retain these obligations: exit-cleanup callbacks cannot
throw, the typed guest activation destructor is `noexcept`, and throwing helpers
inside a native catch use `invoke` with the balancing catch-exit cleanup. For a
callee throw, tag dispatch must search every active lexical handler in the same
native function, inner to outer; native rethrow is valid only after all of them
fail to match, otherwise it would skip a same-function outer Wasm handler. These
native component tests do not qualify Windows SEH, ARM EHABI, the complete QEMU
runtime matrix, or cross-function guest exception support in any CLI mode.

## Foundation integration regression qualification (r209)

Both frozen r209 runtime objects and CLIs were rebuilt after adding the exception
ownership/IR/opfunc foundations and initialized tag-identity roots. This confirms
that the existing supported CLI slice still works; it does not enable or claim
complete cross-function guest exception execution. Each product passes 217
declaration-policy cases plus 72 JIT runtime-module rebuild checks. The suite
still has 37 current exception-syntax cases: ordinary passes 962 UWVM plus 37
Wasmtime runs (999 total), and ROS passes 148 UWVM plus 37 Wasmtime runs (185
total). UWVM counts include validation. Independent feature disabling passes.
Tag-alias cold/warm object-cache qualification passes 30 ordinary and 10 ROS runs.

Four actual memory/memory-loop native-code comparisons per product pass the
complete executable-section and relocation checks: instruction-policy code is
162 bytes and native-unwind code is 130 bytes. Both whole-tree module dependency
and pragma audits pass. Historical 232-pair evidence was not rerun. Production
source fingerprints agree before and after compilation and at final qualification;
the 64-GiB/no-swap cgroup recorded no additional OOM event during this batch.

r209 production source IDs: ordinary
`c4e44600a9c5f548ff58181867c1ec039477e4258ad6a92d3f21446acc55b210`, ROS
`303cecb70f0cade27b73a2384547231679d049afff9806b0d0cc5d27ed8a86f1`.
CLI SHA-256 values: ordinary
`17117963cc92308ad972f4fc22ee0dd9e1f5c72e4411ab942806e8881b1b3d66`, ROS
`14c9db803a5fdc2980435a3831eeed008cd964473af1690f9feed310577f281a`.
Detailed qualification reports remain at
`/dev/shm/uwvm-builds/shared-r209/{uwvm2,uwvm2-ros}/qualification.json`.
The r209 source snapshot and build/test artifacts remain remote. The r208
artifacts were subsequently verified into the local compressed evidence archive
and the corresponding remote regular files were pruned.

## Numeric interpreter exception integration (r211)

The frozen r211 source connects numeric guest exception propagation to pure
interpreter execution. Thirty new command-line cases pass against Wasmtime
48.0.2, including direct/indirect calls, mixed scalar/v128 and large tuples,
ordered/same-frame/outer-frame handlers, loop parameters, imported tag aliases,
tail-call handler bypass, scratch reuse, uncatchable traps and uncaught throws.
Ordinary full, lazy and lazy+verification each pass all 30; ROS full passes all 30.
The runner also checks the precise guest-exception category and the real throw
stack's function order, rather than accepting any nonzero process exit.

These fresh runtime/CLI builds use `-O1` and the default combine/delay CLI profile;
they establish functionality, not release performance. Per product, 62
wasm-tools assembly/validation commands and 30 Wasmtime executions accompany 90
ordinary or 30 ROS UWVM executions: 182 and 122 commands respectively. The earlier
217+72 rebuild fixture and 37-case local-EH suite were not rerun in this focused
batch. An independent compiler unit covers all eight combine/delay settings and
by-reference/one-slot-ring/two-slot-ring dispatch; its results are recorded
separately from these CLI runs.

Uncaught diagnostics use the existing colored `fast_io::io::print` conventions,
show typed raw payload bits, and own the Wasm frame names and indexes captured
before the actual throw unwinds the interpreter stack. Display names are escaped;
instruction/source locations are explicitly unavailable. Retained exceptions
keep that immutable original trace. The host-only callback is loaded only on the
real throw path; normal calls, ordinary operations and memory accesses gain no
trace callback load, allocation, lock or guard. This diagnostic foundation passes
471 trace checks/64 concurrent captures per product in both O3 and ASan/UBSan
builds, plus the existing 3,105 throw checks. Actual module imports, no-exceptions
header use, scoped dependency checks, four throw-opfunc assemblies and the actual
runtime printer in plain/color modes pass. Its complete 177-file compressed
archive is `build/wasm3-evidence/exception-diagnostic-trace-r210-overlay.tar.gz`,
SHA-256 `d434649f0afcb1a2dcdf9e11a32d9a709c06be5b7fa903d11f1eb8dad008031c`.
The r211 CLI runs additionally qualify the actual runtime capture and printer
through uncaught guest throws.

r211 production source IDs: ordinary
`3fe1f2b21683bcbe5d48127957d3c4477a2e78c322d3d779c2bb96491f8a39b4`, ROS
`7bba611a033a502081efec50c44dd088ccbb3aa8aa54b2bfd2a47308aaadfbb6`.
The O1 CLI SHA-256 values are ordinary
`02abdaa102e5c8cd87deff000a7735f0b52068d57e7dbb46478ca9e59f4038a1`, ROS
`bdd5010711e4c210c443eea48e9e93406b3eb2ab5488439fff6e7f83879ea77e`.
The strict command-line runner was kept in an independent test overlay, SHA-256
`4ccbf37f6ff7f6afe2e46bb707601271be92b2065df658b67f7c416f8238030d`;
production fingerprints stayed unchanged before/after builds and qualification.
Reports are in `/dev/shm/uwvm-builds/shared-r211/{uwvm2,uwvm2-ros}/qualification.json`.

Both products were then rebuilt independently at `-O3`, with new runtime objects
and CLIs from the same frozen r211 source. All 30 new CLI cases pass again in the
same modes. O3 CLI SHA-256 values are ordinary
`086b2609d633890659395dc87564ba6b6a056c03ae164cc58243787e4b2fa637`, ROS
`757674143c1a009dd0a055139c8b6414898852b28954dee022e46eaaea19411b`.
The release build manifests preserve every compiler/linker argument. The actual
compiler unit additionally passes 360 executions per product in each of its O3
and ASan/UBSan builds: eight combine/delay settings, three dispatch/cache layouts
and 15 cross-function cases.

The O3 performance runner measures four comparisons with nine alternating AB/BA
pairs each. Both products run sequentially while other agents suspend remote
builds/tests. The original allowed-CPU samples remain intact; because some pairs
were noisy, a separate repeat pins both variants to allowed CPU 0. The repeat
records affinity, background process CPU/RSS, load average and cgroup counters.
Every sample meets the calibrated minimum of 100 ms and 20 times empty startup;
startup and compilation remain included. CPU-0 median paired B/A ratios are:

| Comparison | Ordinary | ROS |
| --- | ---: | ---: |
| Same ordinary call, EH feature enabled/disabled | 1.0004 | 1.0095 |
| Protected normal call / ordinary call | 0.9934 | 1.0003 |
| Local static throw lowering / equivalent branch | 0.9885 | 0.9785 |
| Throw through eight callees / one callee | 4.3278 | 4.4954 |

The first three comparisons show similar median costs in this focused workload;
individual pairs still vary, so these values do not establish universal absence
of regressions or a small speedup. One/eight-callee throw medians are 8.98/39.16
microseconds ordinary and 8.54/38.39 microseconds ROS. Production diagnostic trace
capture stays enabled: these costs include owning value construction, allocation,
frame/name copying, native propagation, tag matching and cleanup. They do not
isolate the C++ unwinder or compare LLVM JIT instruction/unwind policies. These
measurements also do not qualify Wasm memory access or execution with guest
threads enabled.

The benchmark runner SHA-256 is
`63523baffb5cb5fa448fd6a7f4da2df3ccf36d24c21b6007acba6e6a0ddaa293`.
Full raw samples, input Wasm/WAT, commands, strict functional results, fingerprints
and release manifests are retained in
`/dev/shm/uwvm-builds/shared-r211/{uwvm2,uwvm2-ros}/qualification-o3.json`
and their referenced directories. A 1,660-file report/fixture archive was verified
file by file at `build/wasm3-evidence/core3-interpreter-eh-r211-reports.tar.gz`,
SHA-256 `5887a1e5c86f5603e23f4e68a134aa3f1c6e878616f24114e97393f6515df411`.
It is a reports/fixtures archive; runtime objects and CLI binaries remain remote.
No archived remote files were removed during this batch.

This qualification covers numeric exception tuples in the specified pure
interpreter modes. It does not establish the complete reference/exnref tuple ABI,
LLVM JIT or tiered cross-function guest exceptions, full EH conformance, debugger
integration, or performance outside the focused workloads above. Later native emitter changes require their
own source fingerprints and execution evidence.

The separate interpreter metadata foundation passes 1,051 ASan/UBSan checks per
product, with a 58-file verified archive at
`build/wasm3-evidence/interpreter-exception-metadata-r1-r2.tar.gz`, SHA-256
`70ffd42a904be17cac32650dbfae51baed8dc15279b8be4b738069b7bbbb8095`.
Thread-management measurements cover `native_thread_pool` and `execution_domain`
only: each product passes four profiles, nine paired measurements and 32 rounds;
the shared native 8-KiB kernel's optimized assembly has no call, lock or fence.
This is not VM host-entry or Wasm-memory performance evidence. Raw timings and
cgroup records are in the verified 84-file archive
`build/wasm3-evidence/thread-management-performance-r1-r2.tar.gz`, SHA-256
`ef91d6993c1baa0ded1324e3cb68a4c434b82867d77bfca72988ffb9c5f0c4f9`;
the method is `test/0003.utils/thread/creation_performance.md`.

## Native numeric exception execution (r212–r214)

Both products now execute numeric `throw` / `try_table` across generated native
functions, including ordered tag matching, imported aliases, mixed numeric/v128
payloads, loop catch targets, direct/indirect calls and retired tail activations.
The actual Itanium exception activation owns an immutable guest value. Only its
real C++ type is caught; ordinary traps and foreign native exceptions are not
converted into guest catches. This remains a numeric slice: retained exception
references, reference payload reconstruction and the complete GC ABI are pending.

The ordinary r212 CLI passes all 30 new-syntax cases in LLVM full and lazy,
with instruction/unwind/none diagnostics, in both O1 and O3 builds: each run has
272 assembly, reference and UWVM commands. An explicit O3 lazy+verification run
passes another 182 commands; the runner selects the actual mode, not lazy as a
substitute. ROS r213 passes 182 commands for full mode with the same three
policies. Actual imported cross-function tag alias changes also pass cold/warm
persistent cache replay: 30 ordinary and 10 ROS executions. The uncaught cases
require the specific fatal class and exact original throw-stack ordering; `none` explicitly reports that the stack is unavailable.

Ordinary r212 production source ID:
`2505a3a33e7df3f4192c96d4a4262f84008f7792134a6a73b32e901d7c762383`.
Its O3 CLI SHA-256 is
`06cb32c4c938634b62a32acb38253954b33c3157eb6b0060ff9e2a72668169c4`.
ROS r213 production source ID:
`bc1f24e2903c84b2cd9ad7a98ed1f01aef7c7fa97fd975d444d3ed37fa8d084e`.
Its O1 CLI SHA-256 is
`67d90657c18389f63cf83404f1823b77ee414b4ac99ffce6cc9e5e71e7f6df2b`.
The independently rebuilt ROS O3 CLI SHA-256 is
`384fa4ac121fb642c07b9404c0cce2960b4b6d2102936067db9d53009640fe9f`.
It passes the expanded 31-case suite (188 commands), including exact uncaught
negative integer, signaling-NaN, negative-zero and v128 payload bytes, with forced
ANSI colors and resets preserved in raw logs. Ordinary O3 passes those three
strict diagnostics in full/lazy/lazy+verification with all three stack policies.
The O3 ordinary and O1 ROS builds link the genuine LLVM
`23.1.1-uwvm-ros.8` incremental package, with actual RuntimeDyld and version
consumer objects rebuilt; they do not relabel the previous .7 headers/binaries.

Unwind support is selected by host exception ABI, target object format and actual
DWARF CFI capability, independent of the module's exception-syntax switch. A
syntax-disabled module still needs correct unwind tables when an imported callee
throws. Runtime target-machine ownership spans eager/lazy compilation, and cache
identity contains `numeric-itanium-dwarf-native-v2`. Native diagnostic capture
walks physical frames at the throw and resolves the owning JIT module/function;
unwind does not emit or maintain an instruction trace. Instruction mode owns
separate exceptional frame cleanup. Both paths use the formal `fast_io::print`
diagnostic with copied names and an explicit truncation marker when needed.

Actual object checks pass for both products: ten native objects per product cover
ordinary memory, redundant protected normal calls, numeric propagation and tail
transfers. Unused EH feature on/off leaves complete executable sections and
relocations identical. For the ordinary O3 runtime before/after integration,
the unwind memory-loop object remains byte-for-byte identical at 338 executable
bytes; instruction-mode leaf and normal caller through `ret` remain identical,
while the complete object gains 32 bytes of cold exception cleanup (402 to 434).
These code checks are not a timing or whole-program performance acceptance claim.

The ordinary r214 tiered overlay additionally passes 144 actual executions:
88 T1, 48 OSR and eight instruction-mode T2. Cold entry logging proves dispatch to
the published T2 address, including exceptional exits. A fixed periodic caller
could previously miss every hotness sample; the new Weyl-counter helper passes
6,197 checks and an O3 assembly review with no call, lock or extra hot atomic.
The verified 875-entry archive is
`build/wasm3-evidence/tiered-numeric-eh-r212-r214-overlay.tar.gz`, SHA-256
`d964cf44580f9b769a84eede94498af1498fa7fcb09c0c550798f1ff587e41e4`.
This overlay is distinct from the r212 runtime build. Native-unwind T2 remains
unqualified in that archive; its publication/concurrency work is subsequent.

All these builds and executions run only inside the existing SSH Linux cgroup:
64 GiB memory, no swap and CPUs `0,2,4,6,16-31`. The historical OOM counters remain
one event/one kill; no new OOM is implied by these passing runs. The focused O3 native EH measurements and their retained failed calibration
iteration are documented in `runtime/exception-performance.md`; same-Wasm
instruction/unwind throw pairs demonstrate the shallow capture cost rather than
claiming every throw is faster with unwind. The r211 actual VM thread lifecycle
measurements are documented in `test/0017.runtime/wasm_thread_performance.md`,
including short-sample outliers and the longer 21-pair/1,024-round repeat. Those
thread results do not measure the later native EH runtime. Complete runtime QEMU
coverage still requires actual executable tests; previous foundational probes
do not stand in for that acceptance.

## Native EH publication and final numeric diagnostics (r216)

The frozen ordinary source is
`sha256:1d17c4d13e68ec1d6d0425d9b546b066dbdfc0f5c5afb4da862d678c63576dc2`;
the ROS source is
`sha256:66b228d36c4fc17462f3ccbc87217009098abf0b1969ebe2449261c4bbdd8af3`.
Both native runtimes and CLIs were built with O3 and the genuinely rebuilt
23.1.1-uwvm-ros.8 package. The expanded 31-fixture numeric EH runner includes a
new uncaught i32/i64/f32/f64/v128 tuple, preserving signaling NaN, negative zero
and all vector bytes in colored diagnostic output. Ordinary full/lazy/verified
lazy across instruction/unwind/none passes 374 commands; ROS full passes 188.
These counts include fixture assembly and independent reference commands.

Cold T1/T2 publication now orders complete native maps and engine ownership before
callable entries, and prevents late T1 publication from overwriting a ready T2.
Failed symbol resolution discards unpublished ranges. A lazy materialization group
prepares every member's map before exposing any member. Actual tiered qualification
passes 32 native-unwind T2, 24 T1 and 20 OSR executions; see
[runtime/tiered-unwind-publication.md](runtime/tiered-unwind-publication.md).
The earlier r214 limitation above applies to that historical archive, not r216.

The actual concurrent lazy runtime test passes 16 elected-throw processes and one
normal-return control, each with eight native host chains at a guest rendezvous.
Every throw has exactly its original three Wasm frames and elected payload; all
other threads return 3. An initial fixture incorrectly used an int-returning
assertion macro inside a deduced thread lambda, causing undefined behavior on the
successful fall-through path. That test was fixed to an explicit void callable;
its failed evidence is retained. No production normal-return defect was found.

Both products pass actual generated-object checks. The compiler marks only the
known noexcept frame-maintenance bridges nounwind. It can now eliminate redundant
cleanup on proven nonthrowing calls. Against saved r211 O3 objects, the complete
memory-loop executable sections and relocations are identical in both instruction
(402 bytes) and unwind (338 bytes) modes. Protected normal calls also match normal
calls; real throwing calls retain their exceptional edges. Unwind emits no logical
push/pop calls. These are concrete code-generation results, not a claim of zero
cost for every program or platform.

The verified trace/codegen archive is
`build/wasm3-evidence/llvm-native-exception-concurrent-trace-codegen-r216.tar.gz`,
SHA-256 `95a98472d21a9e85d782558ae156066d250e6eade3ecfb858828ec95edd713c9`.
The new mixed numeric interpreter fixture additionally passes 384 executions per
product at O3 across eight combine/delay settings and three dispatch layouts,
using the r216 production headers and the separately identified r217 test overlay.
A general CLI built without combine-op support cannot qualify those settings;
its rejected option is retained and is not counted as a successful matrix run.

## Host control and cooperative debugging in progress (r217–r218)

The two products now share bounded host control framing, explicit launch authority,
move-only request tickets, replay/generation checks and a Linux pre-fork launcher
socketpair adapter. The adapter checks kernel-supplied credentials for every packet
and pins launcher lifetime with pidfd; same UID, the VM's own process and an inherited
file descriptor do not grant control. These are host infrastructure, not a completed
CLI server or debugger. Initial protocol/session qualification passes 9,062 checks
in each product under O3, ASan/UBSan and a no-exceptions build, with real module
consumers. Its verified archive is `host-control-session-r217-r1.tar.gz`, SHA-256
`bd006b65582a9a6b4716a7ec5a7d33bb6e136b85eaf2b91280708654566c28e2`.

`utils/thread/cooperative_pause_domain` coordinates enrolled host executions at
explicit compiler safe points. A pause blocks new admission and cannot be reported
complete until every active participant has parked or returned. Snapshots contain
owned numeric locations, and a stopped publication callback excludes concurrent
resume. Timeouts preserve the outstanding request; close releases waiters before
drain. Initial two-product O3 and ASan/UBSan tests cover eight participants over
128 pause rounds, foreign/stale tickets, host-operation timeout, exceptional exit,
publication/resume exclusion and shutdown. The LLVM full emitter and real runtime
integration are being qualified separately in r218. Step/local inspection, real
CLI console/server integration and function replacement remain unfinished.

## LLVM full debugger console qualification (r223–r224)

Both products now have a host-owned `-m debug-jit` controller with an initial
pre-entry stop, exact Wasm byte-offset breakpoints, cooperative pause/resume,
per-instruction stepping, actual guest-thread backtraces and a bounded `wait`
command. The debug safe-point hook captures a stack only on a requested park,
outside the normal running path. `none` explicitly reports an unavailable
backtrace; `instruction` and supported native `unwind` retain distinct records.
The console owns its input before guest execution. Linux seals the input's OS
identity, rejects guest final-open hardlink aliases before truncation, denies WASI
stdin read rights, and rejects host output aliases before emitting a prompt.
The ordinary r223 WASI-enabled binary passed 9 real observer stops under each of
three trace policies, 10 end-to-end CLI processes, and 5 end-to-end sealed-input
processes. Those fixtures include current Core 3 `try_table`/`throw`, guest
stdin denial, asynchronous exit, regular-file/hardlink access, a controlling PTY,
`/proc/self/fd` path-policy denial, and same-file/FIFO output refusal. The two
path aliases can be rejected by WASI path resolution before final-open checking;
the hardlink case exercises the final-open identity check directly. ROS r225
focused controller and sealed-input tests pass O3, ASan/UBSan and no-exceptions
profiles. Its genuine pinned LLVM `23.1.1-uwvm-ros.9` x86-64 libraries built all
2,903 targets, and the resulting actual O1 VM/CLI passed 9 real observer stops
under each of three trace policies, 6 end-to-end CLI processes and 5 end-to-end
sealed-input processes. The ROS production source ID is
`sha256:8a2efbc399983bb7c94b315f3d4c41fe5905bdfd2f556ea5558f31a19f4f684a`.
This is functional O1 host qualification, not a debugger performance
claim. The ordinary compiler's default-off executable sections and relocations
were bit-identical to r216 in the selected code-generation fixtures.

The local controller currently has no general server, secure late activation,
locals inspection or function-level replacement. Full Core 3 GC/reference
and retained exception-reference semantics remain incomplete. The LLVM `.9`
RISC-V ABI patch passed its isolated QEMU cases; the actual `.9` RISC-V VM
tail-call/thread qualification is recorded below. The full cross-target matrix
still requires qualification.

## Genuine LLVM `.9` RISC-V VM and stopped memory inspection (r227–r229)

The pinned ROS LLVM `23.1.1-uwvm-ros.9` RISC-V build completed all 2,280
targets. Its 62 actual product-link archives were hashed individually, and the
12,874-file source manifest has SHA-256
`cc680aa09a6fb23fe1a9ca2746a51a029d635386e17fb9ac7d264b6e9971a8e1`.
The actual ROS VM built from source ID
`sha256:306c3124b698f36d1c87542af5715a063634eb4ab4f005e372f5f23c1a6119f5`
passed RISC-V QEMU tail-call suites: 1,000,001 self/mutual transfers,
cross-module and host tails, indirect differing-signature/tuple transfers,
concurrent bounded host stacks, and `instruction`/`unwind` policies. The same
provider passed actual VM atomic bridges, `wait32`/`wait64`/`notify`, shared
size/grow, concurrent reset and managed wait cancellation under both policies.
Three separate native EH MCJIT probes passed on RISC-V QEMU: 53 symbol checks,
18,313 product-landingpad checks with 1,152 executions, and 13,941 host-binding
checks with 1,536 executions. An actual RISC-V ROS LLVM-full CLI built from the
same qualified runtime and `.9` libraries then passed 31 Core 3 cross-function
exception cases in 157 commands, including Wasmtime v48.0.2 comparisons and
`instruction`/`unwind` policies. The matrix covers `try_table`/`throw`, imported
tag identity, indirect calls, tail bypass, traps, and readable colored uncaught
diagnostics. This qualifies RISC-V exception execution, not the remaining
platform matrix or retained `exnref` semantics.

Both LLVM-full debuggers now accept `memory MODULE MEMORY OFFSET LENGTH` only
after a complete cooperative stop. The console limit is 256 bytes. The host
runtime resolves the indexed memory and rechecks the range against its backend
snapshot before copying; the ordinary generated Wasm memory path is unchanged.
The r231 ordinary source ID is
`sha256:d200e2cecf6e8e7c4c7a0c832f3c61b0931120da1ffa4423401847057c14168b`
and ROS is
`sha256:79ee5d80e331bd48bfef03dd4c7fef2444560e3c0bf88db9187daeb2dcd8addc`.
Focused controller O3, ASan/UBSan and no-exceptions profiles passed 810 checks
per profile in each product. Actual CLIs passed 11 ordinary and 7 ROS processes,
including bytes read at a Core 3 `throw`/`try_table` stop, separate memory32 and
memory64 indexed data segments, range and parser bounds, zero-length end handling,
and rejection after guest exit. The indexed data syntax and address types follow the
[Core 3 text module rules](https://webassembly.github.io/spec/core/text/modules.html)
and [memory type rules](https://webassembly.github.io/spec/core/text/types.html).
Sealed-input CLI tests passed another 5 processes
per product. This is functionality qualification; full debugger and memory-access
performance acceptance remain open.

## Controlled r231 `-O3` exception, memory and VM-thread measurements

Both products were rebuilt on SSH Linux inside the required 64 GiB/no-swap
cgroup from their already-qualified r231 O1 commands, changing host compilation
to `-O3`; generated full-JIT code used `pb-o3`. The source fingerprints stayed
unchanged across each build: ordinary
`sha256:d200e2cecf6e8e7c4c7a0c832f3c61b0931120da1ffa4423401847057c14168b`,
ROS `sha256:79ee5d80e331bd48bfef03dd4c7fef2444560e3c0bf88db9187daeb2dcd8addc`.
ROS linked the genuine bundled LLVM `23.1.1-uwvm-ros.9`. Each actual O3 CLI
passed three newly added Core 3 cross-function exception cases in 17 commands
under `instruction` and `unwind`, including a mixed tuple, tail bypass and a
colored detailed uncaught payload. Each also passed eight actual native
exception/mmap code checks over ten cached objects and 82 commands. The mmap
leaf retains a real scalar store/load without an extra helper call, conditional
guard, lock or fence in unwind mode; the protected caller leaves that leaf's
machine code unchanged. This is selected generated-code evidence, not a claim
that all memory workloads have no regression.

Nine alternating AB/BA pairs per comparison, at least 100 ms and 20 times
process startup per sample, measured the same binary with exceptions enabled
versus disabled and a normal call in a protected versus ordinary body. Median
enabled/disabled ratios were 1.0183/1.0131 (ordinary instruction/unwind) and
0.9835/1.0092 (ROS instruction/unwind). Protected/ordinary ratios were
1.0064/1.0018 and 1.0262/1.0108 respectively. Matching a real cross-function
throw at one and eight call levels on identical Wasm and loop counts gave
paired unwind/instruction ratios of 1.7209/0.9755 ordinary and 1.6116/0.8552
ROS. These ratios include native diagnostic capture, propagation, startup and
JIT work; they are not isolated unwinder timing or a general speed guarantee.

Fresh O3 runtime objects also passed real VM thread-entry semantics, native
creation counts, shared-memory `atomic.fence` work and checksums under both
stack policies. An otherwise idle CPU0 then ran 21 samples of 1024 rounds per
configuration using the same hash-verified timed binaries. Median extra wall
time for fresh-thread versus same-thread VM entry was approximately 12–13 µs
for one worker and 53–55 µs for four workers, across full/raw APIs and both
products. Four workers shared CPU0, so this characterizes lifecycle and
scheduling overhead, not multicore throughput. Exact raw samples, resource
snapshots, build commands, source IDs and 630 per-product file hashes are in
`build/wasm3-evidence/ordinary-o3-r231.tar.xz` (SHA-256
`804cc77100ca244f80643483bd402aa26bed86d2534c286f0c6bca17032bc288`)
and `build/wasm3-evidence/ros-o3-r231.tar.xz` (SHA-256
`93f1147fb5eddcf8f01d25ccbf8b92a0890f4390b7c2d51fffdecaab320a94ba`).
Both archives were extracted and checked against every live file before the
derived build directories were removed. This qualifies controlled r231, not
later local working-tree edits or the complete acceptance matrix.

## r232 Core 3 syntax and execution checks

The next immutable Linux snapshot retained ordinary source ID
`sha256:b8e127df845871f4c8df997e7685f72c09a9d5b1447f4813e2d6fbc87a993639`;
ROS retained its r231 source ID. Both products passed the complete header/module
dependency checker. The ordinary product passed ASan/UBSan recursive-binary
(2,626 boundary cases), registry (50,000-deep and 100,000 randomized ancestry
checks), recursive validation (50,190 checks), local declarations, reference
typing/casts and all 31 GC-prefixed immediate encodings (1,524 checks). Exactly
150 new Core 3 function bodies covering typed references, casts and GC typing
agreed with Wasmtime's validity result; nine cross-module recursive-linking
cases also agreed. These are validation tests, not GC execution support.

The actual ordinary O1 LLVM-full CLI passed 31 newly added exception cases in
188 commands across `instruction`, `unwind` and `none`, including colored
uncaught diagnostics. The interpreter full/lazy/lazy-with-verification CLI passed
selected new exception cases; selected tiered T1, OSR and T2 runs entered real
native code. A separately rebuilt CLI compiled with all interpreter combine
levels passed the complete 31-case exception fixture in **839 commands** across
all three interpreter modes and eight combine/delay configurations. The ordinary
debugger observer passed nine real instruction stops per stack policy. The
entire r232 evidence set (3,211 files) was hashed in place, archived, extracted
and hash-checked again, then copied to
`build/wasm3-evidence/r232-qualified.tar.xz` (SHA-256
`fa65ad130774fc2fbaa9196a94d5821017d1c2b039d5a7cec35f5a8c81d1317e`).
ROS exception execution and the remaining acceptance matrix are not established
by this r232 run. Subsequent r233 source changes require their own qualification.

## r233 validator and RISC-V cache ABI checks

The next isolated source snapshots passed both complete header/module dependency
scans and both ASan/UBSan recursive-type suites. Their source IDs were ordinary
`sha256:480bf37fce01f1d9d44edc561171b8faafb86b946870436eafee5c5f1ef691e8`
and ROS `sha256:f1f49ffe02e43e99eead9f74c9caf68e9c07ad001eb4359a9748cc29eed934b2`.
ROS additionally compared 150 exact new Core 3 function bodies against Wasmtime:
20 reference instructions, 55 cast/conversion/function-label cases, and 75
aggregate/i31/reference-equality cases. All results agreed. The 1,610-file
validation archive was checked against both live files and an extracted copy;
`build/wasm3-evidence/r233-validation.tar.xz` has SHA-256
`8019dd26d3aa67350f6b57094ccf28d1a159794df99194d403d3abd2865d7c46`.

The ordinary LLVM code can opt into repaired RISC-V TailCC while keeping the
same LLVM version string. That changes the private typed-call ABI, so both
products now encode qualified versus unqualified RISC-V TailCC in the native
cache fingerprint and use schema v26. ROS imports its pinned LLVM capability
definition into both cache header and module builds. Cross-compiled Linux/QEMU
checks found that toggling the ordinary macro changes exactly this ABI field;
the patched ROS provider reports qualified. The source patch and three QEMU
binaries/results are in `build/wasm3-evidence/r233-riscv-tailcc-cache-key-audit.tar`
(SHA-256 `6eaa5730f62b840a46387009e56b2da2d1bc6ce5d2626e8d0fd656664679542c`).
These checks establish cache-key separation, not complete native RISC-V tail
execution or general cache acceptance.

## r234 O3 execution, code generation and performance

The immutable r234 source IDs were ordinary
`sha256:d92d44d0b854a88e3152cb9f4d23d91b2de1cccf1f8b7d96cc3d2891d0d3cbe3`
and ROS `sha256:cac1b1ccf372320701c3f636ac361bd3b09ae55252a23ee0b903dd24f6727f5e`.
Both actual O3 CLI binaries were freshly built in the 64 GiB, no-swap Linux
cgroup using their recorded runtime objects; ROS used pinned LLVM 23.1.1-uwvm-ros.9.
The source, compiler, cgroup and object hashes were checked before and after
linking. New syntax exercised native `throw`/`try_table`, imported tags, typed
tail calls, indirect tuple returns and retired-frame traces. LLVM full passed
84 tail cases and 31 exception cases per product. The ordinary CLI additionally
passed 126 LLVM lazy/lazy-verification tail cases, 78 interpreter full/lazy/
lazy-verification tail cases, 48 selected tiered configurations, and all 31
exception cases in its LLVM lazy and three interpreter modes. ROS interpreter
full passed 52 tail cases and 31 exception cases. Both products passed the
21-case cache/memory-backend suite and 8 native exception/mmap codegen checks.
The generated O3 mmap scalar load/store leaf machine bytes were identical to
r231 for both products under `instruction` (34 bytes) and `unwind` (22 bytes),
with no new call, lock or fence in either leaf.

Nine paired AB/BA performance samples per comparison on CPU 0 passed the
benchmark's semantic and duration checks for three comparisons per product and
stack policy. The median B/A ratios for `unused-feature` and
`normal-protected-call` were respectively 1.035/1.030 (ordinary instruction),
1.012/0.995 (ordinary unwind), 0.997/0.982 (ROS instruction), and 1.007/1.006
(ROS unwind). The deeper native-unwind fixture took 2.787, 1.613, 2.808 and
1.642 times its shallow fixture respectively; those are *different call depths*,
not a direct measurement of instruction-stack overhead. A fourth
`local-static-lowering` calibration failed because O3 made the reference path
too fast to reach the required 100 ms sample before the fixture's signed i32
iteration cap. Its failed record is retained; no performance conclusion is
drawn from it. These process-level measurements include startup and JIT work
and do not establish a general no-regression guarantee.

Both fresh O3 binaries passed actual interactive `-m debug-jit` CLI cases:
ordinary 11 and ROS 7 processes, including EH stepping, bounded memory reads,
WASI input isolation and fatal incompatible-mode diagnostics. A separate sealed
input check passed five processes per product for procfd, hardlink, PTY and
output-alias rejection; its verified 39-file archive is
`build/wasm3-evidence/debug-sealed-cli-r234.tar.gz` (SHA-256
`9cca432371d0b12aac24e2fcc305dd8ee9ab4cf1d63e59fc0f73f4c1973a0938`).
The complete r234 O3 build and test output was checked file by file before and
after archival in `build/wasm3-evidence/r234-qualified.tar.xz` (SHA-256
`9bdf3144ecb524b94102da4de5ebc603c153eead7c5035dfaed9cf70e3f84e21`).

## r235 real VM thread requalification

A provenance-checked relinker built new timed and native-thread-counting
fixtures against each r234 O3 runtime object, with unchanged production source
IDs and exact prior successful LLVM/host ABI flags. The qualifier passed
`instruction` and `unwind` with actual native thread counts and matching
guest/shared-memory checksums in both products. Then the uninstrumented timed
fixtures completed 21 samples of 1,024 rounds for each policy on CPU 0. The
paired median threaded/same-thread wall ratios for one/four workers were:

| Product / policy | Native C++ | VM full entry | VM raw entry |
| --- | ---: | ---: | ---: |
| ordinary / instruction | 2.999 / 3.263 | 1.925 / 2.089 | 1.996 / 2.096 |
| ordinary / unwind | 3.073 / 3.282 | 1.920 / 1.999 | 1.925 / 2.011 |
| ROS / instruction | 3.100 / 3.293 | 1.901 / 1.999 | 1.929 / 1.985 |
| ROS / unwind | 3.034 / 3.228 | 2.017 / 2.105 | 1.996 / 2.113 |

These ratios compare creating threads to running the same guest work on the
current thread within each path. They are not a cross-revision regression
measurement and do not isolate thread creation from guest entry, imports and
scheduling. The binaries, exact commands, qualifiers, all raw samples and
before/after source fingerprints were verified file by file and archived in
`build/wasm3-evidence/r235-thread-qualified.tar.xz` (SHA-256
`68dd4024a9a74f227715e27a88ca10a2997abbca322efd5086e23dc5aa488135`).

## r239 corrected local exception performance fixture

The r234 `local-static-lowering` calibration failure exposed a fixture issue:
the O3 compiler could largely remove its local throw/branch loop, making the
timing mostly process/JIT startup. Its earlier attempted measurements are not
execution-performance evidence. The corrected fixture retains the i32 LCG
work, uses an i64 loop count, and performs the same sequentially consistent
atomic store after every iteration on both sides. A final atomic readback and
guest checksum check verify the work. Wasmtime validated and ran both new
Wasm syntax variants with threads/shared-memory support. This extra common
atomic work prevents loop elimination but means the ratios do not isolate
exception overhead from the shared work.

Against the exact archived r234 O3 binaries, nine AB/BA pairs per policy on
CPU 0 completed above both the 100 ms and 20x startup thresholds. The median
`throw`/`catch` divided by equivalent branch ratios were 0.9992 (ordinary
instruction), 1.0064 (ordinary unwind), 1.0067 (ROS instruction) and 0.9915
(ROS unwind). The minimum individual samples were respectively 141, 176, 153
and 163 ms. Failed pre-correction calibration records, the new exact runner,
modules, reference runs and all raw pair timings were verified and retained in
`build/wasm3-evidence/r239-eh-perf.tar.xz` (SHA-256
`84a0c983571b7091c569da258dac8e5dc9ef64758afff90df9cfd6955db905b8`).

## r241 O3 debugger locals, unreachable `throw_ref`, and hot-path checks

Fresh Linux-cgroup O3 CLI builds used immutable ordinary source ID
`sha256:a1e2120e9364fd8ee1bca857fdc49986dda51fa4782c259b5b485ed6f5d30c40`
and ROS source ID
`sha256:da1513608935b93479bcf267df4b5c83004bf04bda2e8d6aeb63975b3e4ea765`.
The LLVM-full interactive debugger now supports `locals THREAD` at a complete
cooperative stop. It reads bounded snapshots of actual integer, float-bit,
SIMD and reference local values. Ordinary debug CLI tests passed 12 processes,
including Core 3 `try_table`, SIMD and references; protocol/controller checks
passed 8,106/922. ROS passed 8 CLI processes and 4,894/859 corresponding
checks. Non-debug LLVM-full code generation retains its ordinary path.

The Wasm 3 validator and all requested translation scanners accept `throw_ref`
in a validated unreachable region and preserve its feature gate. Reachable
`throw_ref` still fails explicitly because retained `exnref` values do not yet
have an owning runtime ABI. Wasmtime accepted all 18 initial syntax fixtures.
The ordinary full/lazy/lazy-verification interpreter and LLVM-JIT modes passed
120 product commands; ROS full interpreter/JIT passed 40. Eight actual native
executable-section and relocation comparisons in both products were identical.

A separate ordinary O3 build compiled all three combine and both local-delay
macros into the runtime and CLI. The expanded 20-case `throw_ref` fixtures
passed 528 commands across three interpreter modes and eight configurations.
The 31-case cross-function Core 3 exception set passed 744 UWVM commands in
that same matrix, with 31 Wasmtime reference executions. Operation logs showed
the delay-enabled test changed one function from seven operations/88 bytes to
six operations/80 bytes while preserving its result. Earlier r232 combine
evidence did not compile the two local-delay macros, so its delay-switch
results alone did not establish this path.

Both r241 binaries passed eight native EH/mmap codegen checks each. Their actual
mmap scalar read/write leaf instruction bytes match the r234 archived objects
in both stack policies and both products, including the leaf called from a
protected `try_table` body. The native-unwind leaf contains no extra call,
software guard, lock or fence. Nine paired O3 timing samples per comparison on
CPU 0 passed the 100 ms and 20× startup thresholds for unused feature,
protected normal call and corrected local throw/branch. The median B/A ratios
were respectively 0.984/0.993/1.025 (ordinary instruction),
1.027/0.993/0.997 (ordinary unwind), 1.003/0.979/1.003
(ROS instruction), and 1.006/0.986/0.990 (ROS unwind).
The corrected local fixture includes a shared atomic store on both sides;
these ratios include startup/JIT work and do not isolate exception overhead
or prove a general cross-revision throughput guarantee.

The 3,281 retained r241 files, including build manifests, raw commands,
reference runs, failed preflight/unsupported-combine attempts, all samples and
native objects, were checked before and after archival in
`build/wasm3-evidence/r241-qualified.tar.xz` (SHA-256
`f0d65c09667dd35d798a2b1348df9e63d3b1679757a06b2c1b50193a61f52da4`).
The later host-tail exception stack regression identified by r242 is a
separate, still open fix at the time of this r241 qualification.

## r243 host-tail native-unwind stack correction and performance

The r242 genuine host-import/public-raw-reentry probe exposed a native-unwind
error in both products. A direct `return_call` incorrectly reintroduced retired
Wasm function 2 (`[1,2,3]` versus the instruction policy's `[1,3]`). An
indirect host tail also misplaced or omitted its live imported-host frame 0.
The fix records exact loaded JIT function ranges and the native host-reentry
CFA boundary during cold trace capture; generated Wasm call/memory hot paths
were not changed. The r242 probe also needed two corrected expectations: the
live host import is a legitimate indirect frame, and the normal diagnostic
phrase about unavailable source locations is not a missing-stack warning.
The original failing outputs and corrected oracle remain separately archived.

Fresh r243 O3 binaries from source IDs
`sha256:975399904f7e29c97208e0d38c2f4515ce3350bd66101aca77ee49c6038cd5a2`
(ordinary) and
`sha256:7f44d9d0812f9febb636f29a35920dd7139cfa94736a1236ac42630e51743f8d`
(ROS) passed 16 genuine host-reentry executions: direct stack `[1,3]` and
indirect stack `[1,0,3]` under both instruction and unwind policies in both
products. Another 186 product executions of 31 Core 3 exception cases passed
in ordinary LLVM full/lazy and ROS full, each with both policies. The 552-file
build/probe/test archive is
`build/wasm3-evidence/host-tail-reentry-r243-qualified.tar.xz` (SHA-256
`3669e5e3e092abc5092de1de513909e897f9313361a41d7fedbb655f19715ee3`).

Both new binaries passed eight actual native EH/mmap codegen checks each. Four
mmap scalar-memory leaf byte comparisons per product against r241 matched
exactly across instruction/unwind and ordinary/protected callers. For the same
8-deep cross-function `throw`/`catch` guest binary, nine CPU-0 AB/BA
before/after O3 pairs per product/policy passed the 100 ms and 20× startup
thresholds. Median r243/r241 whole-process ratios were 1.003/1.021
(ordinary instruction/unwind) and 0.983/1.008 (ROS instruction/unwind); the
shortest sample was 822 ms. These are bounded measurements including launch
and uncached JIT, not an isolated unwinder cycle count or universal no-regression
proof. All 470 codegen, module, reference and timing files were verified in
`build/wasm3-evidence/r243-codegen-performance.tar.xz` (SHA-256
`334f03e153043453b109f43c0fa42ef873b34d4ae0127f4d93732e3f715b2525`).

## r244 frozen-CLI threads syntax regression

The exact r241 O3 binaries, without a rebuild, passed 928 actual guest
executions, 157 feature-disable rejections and two malformed fence-immediate
product rejections. Another 226 Wasmtime reference executions passed. Cases
include all atomic load/store widths and RMW families, wait/notify, a fence
with live stack operands and no memory, indexed shared-memory size/grow and
new-page zeroing. Three pinned upstream Wasmtime thread files supplied 17
additional assertions. Ordinary execution covered interpreter and LLVM full,
lazy and lazy+verification, plus four tiered configurations; ROS covered both
full backends. Tests exercised `instruction` and `unwind` where LLVM applies.
The tiny tiered cases established the selected launch/compiler path, not a
real T2 promotion. No real sleeper was awakened by a second guest thread in
this batch; concurrent wake behavior has separate host-level evidence.

The 2,461 files were individually verified and archived in
`build/wasm3-evidence/r244-threads-cli-qualification.tar.gz` (SHA-256
`b1f3bf62f36b8f15ba4c2e163395ae99b5ac7ecc00d8dfa04b89da845225a01a`).
This targeted batch is neither the full official threads suite nor the QEMU
platform matrix.

## r246 Linux late debugger control capability

Both LLVM-full products now accept an explicit Linux
`-m run --debug-jit-control-fd N` launch option. A trusted launcher passes one
end of an anonymous Unix `SOCK_SEQPACKET` pair; only its process may send
bounded commands after the guest starts. Adoption and every packet check peer
credentials and a live pinned PID, reject descriptor passing and oversized
messages, and seal the VM endpoint against guest WASI/procfd access. The
management endpoint closes on detach while the guest continues. The switch
enables JIT instruction safe points at launch; without it, ordinary full-JIT
compilation and execution do not create the controller. Full interpreter,
lazy/tiered, invalid-FD and aliased-stdio launches fail with mode/authority
diagnostics. This is a prearranged control capability, not arbitrary attachment
to an unprepared process or a network server.

Fresh O3 source IDs were
`sha256:8777fac038d448fbb658a0b4d3c00eb32ad225b77bcfec3fa8faf693008a8ec8`
(ordinary) and
`sha256:1e705c335fdbe359cf5fd82d57745bc5af8c08956e79e3f2e911a29211101ff2`
(ROS). Actual Linux-cgroup launcher tests passed 13 ordinary and 11 ROS
processes, including commands after a running guest marker, Core 3
`try_table`/throw/catch, pause/backtrace/continue, guest FD denial, detach EOF,
malicious ancillary data and peer exit. The 70-file build/test archive was
verified item by item:
`build/wasm3-evidence/debug-attach-r246.tar.gz` (SHA-256
`5689bfbcb35ede470eaf43158dfa880cbf13284a417a04f664433925fd810ca9`).
With the option absent, both binaries also passed eight native EH/mmap
codegen checks each and four exact mmap leaf byte comparisons per product
against r241. A network server and function replacement are still missing.

## r247 qualified patchable-call groundwork and integrated regression

The optional LLVM-full debug-only compiler path now emits acquire loads from a
host-owned typed-target array for direct, indirect and self-tail local calls.
It is not connected to a replacement command or live code publication yet.
The compiler probe compiled against matching O3 runtime objects in both
products. It checked 20 actual native objects, 52 rejected invalid/mode
configurations, five slot loads and five `musttail` sites per enabled policy,
including real indirect tail jumps. With the option disabled, eight complete
executable-section and relocation comparisons against r243 were identical.
The first failing probe only assumed LLVM's constant GEP was an instruction;
the corrected test accepts LLVM's constant expression without changing
production code. The 487-file compiler qualification archive is
`build/wasm3-evidence/r247-debug-patchable-compiler-qualification.tar.gz`,
SHA-256 `aa31652e9dbe00ac80ca0619cb0fa69ccd5c6b61b6cc185a2bb194482821ec60`.

Matching O3 CLIs were linked from the frozen r247 source IDs
`sha256:75c8528be908555515410a52fc232a609a2c2588a5b7a0db4d3d224f8c51fa51`
(ordinary) and
`sha256:0e50d182fe617a2fcf47a370ea00c206529d71e70d4f7f7c861a2a7f8b410e83`
(ROS). Their SHA-256 values are respectively
`d4fa164dd2b17cf043ed11991ee51a5b3d926a9acf71104f6d830e8a7434b7af`
and `85384cb785143ad5242a2a0587e90c925c75f710b87326cf5f35fefae7c18388`.
The late control-FD suites pass 13/13 and 11/11 processes. New Core 3
cross-function exception syntax passes 31 cases/219 commands ordinary and
31 cases/157 commands ROS, including Wasmtime references and both LLVM
instruction/unwind policies. Three pinned Wasmtime threads files exercise 17
wait/notify assertions across 119 ordinary and 51 ROS executions. Each product
passes eight native EH/mmap object checks; all four actual mmap memory-leaf
machine-code comparisons per product are byte-identical to r243.
The matching r247 default O3 interpreters also pass the 31 cross-function
exception cases in ordinary full/lazy/lazy+verification (188 commands) and ROS
full (126 commands). The optional combine/delay flags were not compiled into
those default CLIs; the separate r241 O3 combine build already passed these
same 31 cases across all three ordinary interpreter modes and eight
combine/delay configurations (839 commands). A failed attempt to apply those
flags to the r247 default CLI is a build-profile mismatch, not a guest failure.
The 475-file interpreter supplement, including that failed flag invocation,
is `build/wasm3-evidence/r247-interpreter-core3-qualified.tar.gz`, SHA-256
`9f475688dd672a579c6eaba2f2556ea612d58aaa54d499244cc7aede635ef99e`.

Separate r243 O3 binaries also pass the same 17 pinned threads assertions in
ordinary interpreter full/lazy/lazy+verification (68 executions) and ROS
interpreter full (34 executions). The 404-file evidence archive is
`build/wasm3-evidence/r247-thread-official-qualified.tar.gz`, SHA-256
`6597a4de348f10c15d34703aec9abecaa65bdc9e970a8e2410da84305d51def3`.
The 1,306-file r247 integration and performance archive is
`build/wasm3-evidence/r247-integrated-qualified.tar.gz`, SHA-256
`5b69a000696894ced03ef3f0091a41da2588bbfe77c427f1b6790015e6fd4b96`.

Sequential CPU-0 O3 AB/BA measurements use nine pairs each, the same eight-deep
Core 3 throw/catch guest, and at least 0.83 seconds per measured process. Every
sample exceeds 20 times startup duration. Median r247/r243 elapsed ratios are
ordinary instruction 1.0080, ordinary unwind 1.0042, ROS instruction 0.9866,
and ROS unwind 1.0243. These finite process-level samples and the exact native
memory-code comparisons show no large regression in this change; they do not
prove all workloads or architectures are unchanged.

Existing r227 ROS RISC-V LLVM-full code was additionally rerun under QEMU on
Core 3 cross-function exceptions and new threads atomic load/store/RMW and
wait/notify syntax with Wasmtime references in both stack policies. Its
378-file archive is `build/wasm3-evidence/qemu-core3-threads-riscv64-ros-r227.tar.gz`,
SHA-256 `16fc0548aa7e9f3686cbf32af8e4be237e22ad9f08ce4e2616b28e82aefeed5a`.
That binary predates r243/r247 and was built with mmap disabled; this rerun is
syntax evidence, not current-source QEMU or mmap-performance acceptance.

## r249 reachable exception-reference validation preflight

Both standalone Wasm 3 validators now distinguish nullable exception references
from function/extern references when validating `ref.null exn`, `throw_ref`,
`ref.is_null`, and `ref.as_non_null`. Exception-reference instructions remain
subject to their own opt-in feature switch. At r249 this was **validation only**;
the limited reachable null trap added later is documented below. General
reachable `throw_ref` and `catch_ref` still require an owning exception-reference
ABI. The focused Linux
cgroup O1 ASan/UBSan validator cases passed 32/32 per product; guard-page
decoder cases passed 2,463/2,463; and nine current-syntax Wasmtime
`throw_ref`/`catch_ref`/`catch_all_ref` reference cases passed, including the
specified null-reference trap. The 1.9 MiB archive
`build/wasm3-evidence/exnref-preflight-r249-qualified.tar.xz` has SHA-256
`dcd3ddbc983af9f326652a47188e56d0dca5f4e1cd0531ff2af967c36667dcb9`;
each archived file was hash-checked. These results do not qualify reachable
exception-reference execution in UWVM.

## Host-owned debug server qualification

Both repositories now ship `tools/debug/secure_server.py`, a local Unix
`SOCK_SEQPACKET` server/client that supervises the existing, launch-authorized
Linux debug control FD. The VM still has no uninvited attach endpoint. The
supervisor keeps the VM's original authenticated peer process alive, stores a
fresh 256-bit capability only in an owner-only directory, checks the client's
credentials and pidfd on each packet, rejects VM-PID, wrong-capability,
oversized and descriptor-bearing packets, and rejects any explicit guest WASI
directory preopen that would expose the capability file. `disconnect` releases only the
client; authenticated `quit` detaches control while the guest continues.
There is no TCP listener or working function replacement.

The test `test/0017.runtime/run_llvm_debug_server.py` ran a **new Core 3
`try_table`/`throw` plus shared atomic store/load guest** under the matching
r247 ordinary and ROS O3 LLVM-full CLIs. Each product passed 8/8 server checks
after the guest's running marker, including pause/backtrace/resume, rejected
peer/capability/rights, unsafe preopen, reconnection, client-local disconnect
and host detach.
All tests ran in the SSH Linux 64 GiB cgroup. The evidence archive is
`build/wasm3-evidence/r247-debug-server-qualified-v5.tar.gz`, SHA-256
`735e8e8b6a4bc798f745d9da4e906fc2c79398cd3dff259b6f5d1ca7721eed91`.
The same suite also passed 8/8 on each r251 source-matching O3 CLI. Its
test archive `build/wasm3-evidence/r251-debug-server-qualified-v5.tar.gz`
has SHA-256 `9d9a25178420cf33a4165774a40bf3ef4d2403ba675a3bacc12323188df9f7d5`;
the complete ordinary/ROS r251 native outputs are separately preserved in
`build/wasm3-evidence/r251-debug-full-native-qualified.tar.gz` (SHA-256
`2d894c90551c4bc3db347cde0e2bce121a2fb99c8ce78e05471d4e6369a7aa87`,
108 archived files checked against their individual hashes).

## Historical work list at r251 (superseded by later qualification)

- Complete Core 3 type system and standalone validator: typed/function references,
  recursive types/subtyping, GC, non-nullable references, and definite local
  initialization; corresponding parser/storage/initializer changes.
- Complete general JIT tail calls, exceptions/tags/try_table, memory64/table64, and all
  remaining Core 3 validation/execution rules in each requested backend/mode.
- Threads: finish guest wait/notify and mixed atomic bridge qualification,
  cross-module concurrent sharing, initialization synchronization and host shutdown
  behavior, then complete official conformance across requested execution modes.
- Complete the debugger's missing behaviors and function-level same-ABI hot
  replacement. The local interactive console, bounded stopped locals inspection,
  prearranged Linux control FD and host-owned Unix server work. Arbitrary
  attachment to an unprepared VM and a TCP listener remain unavailable.
- Exception-aware instruction/unwind stack handling, complete official differential
  testing, expanded generated-assembly checks, and full runtime QEMU acceptance.
- No WASIp2/WASIp3 implementation is part of this work.

## Debugger and replacement design constraints

These constraints cover both the implemented host-owned server and remaining work.
WAVM's `Lib/LLVMJIT/EmitModule.cpp` uses platform personality functions. UWVM's
existing unwind registration is in
`runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h`.
Stack-trace unwinding alone does not implement guest exception handling.

Late activation must be explicitly enabled by a host launch option. A management
Unix socket should live outside guest preopens in a private directory, require
peer credentials plus a fresh capability, and keep that capability out of guest
arguments, environment, descriptors and logs. The host supervisor stores its
client capability in an owner-only file; the VM receives only an inherited
socketpair endpoint and never the client capability. A guest must not gain
control by connecting to its own management socket or sending a signal.
Remote TCP requires authenticated
transport; loopback alone is not authorization. Native host plugins remain trusted.

State-changing commands must stop guest threads at defined safe points. Wasm
faults/exceptions remain distinct from debugger events. Native PCs need stable
module/function/source/generation mappings, including optimized frames.

Replacement must validate the complete Wasm function type and runtime ABI before
compilation/publication. Direct callers, inlined copies, tables and function
references must remain consistent. Keep code and unwind records alive while any
frame or debugger snapshot refers to that generation. Publication must be atomic;
failure preserves the old function. Non-full-JIT modes and unsupported targets
must fail with the established fatal diagnostic format.

## r251 bounded reachable null exception-reference execution

Both interpreter products now recognize the immediately adjacent Core 3 sequence
`ref.null exn/noexn; throw_ref`. The compiler retains a distinct static exn
token only until the next checked opcode and emits a terminal null-reference
trap without creating a native guest exception or materializing a borrowed
exnref in the packed operand stack. Thus `try_table` cannot catch the trap.
The exception feature switch gates both heap spellings; other reachable
exception-reference uses, including non-null rethrow and `catch_ref` /
`catch_all_ref`, still fail explicitly. LLVM JIT still rejects reachable
`throw_ref`. This is a deliberately bounded implementation, **not** complete
Core 3 exception-reference support.

On the SSH Linux 64 GiB cgroup, source-matched r251 O3 binaries passed four
new-syntax cases against Wasmtime 48.0.2: ordinary uwvm-int full, lazy, and
lazy+verification 22/22 executions, and ROS uwvm-int full 10/10. The cases
include `ref.null exn`, `ref.null noexn`, a numeric stack prefix, and a null
trap inside `try_table`; exceptions-off rejection is also checked for both
heap spellings. The updated dead-`throw_ref` compatibility suite passed 20
cases / 89 commands ordinary and 20 cases / 43 commands ROS. Evidence, raw
logs, test script, and matching build manifests are in
`build/wasm3-evidence/r251-core3-null-exnref-both-qualified.tar.gz`, SHA-256
`3e451d883bdcf1c4a9ca4f392e5de760f26bcdaf707c73d112ecc0bf88f37e32`.

## r252 debug-full live-table routing and current native qualification

Debug-full now allocates a stable typed target slot array before compiling each
eligible module and publishes every nonzero entry before guest execution.
Direct and tail local calls read those slots; `call_indirect` resolves a copied
target from the **live** table under a host lock shared with table mutation and
ordered active-element application. This avoids freezing null/old elements when
debug preparation compiles before later module initialization. The lock is
released before entering guest or host target code. All debug-full JIT object
cache reads and writes are disabled because its generated code embeds
process-local slot, observer, and pause-domain addresses. Ordinary full-JIT
still uses its prior code generation and cache path. No function replacement
request publishes a target: `replace_function` remains explicitly unsupported.

Both r252 source-fingerprinted Linux-cgroup O3 CLIs built and ran: ordinary
source ID `sha256:7be0b248dfe536887f444560d8e3e2e7f14875b9d55c1c4cff7bb27877f341a9`,
ROS source ID `sha256:a2b7934eaa60eb00da621321553bdc8e72a248723b5a298d6bbba00d343135f3`.
The actual debug console passed 22 ordinary and 18 ROS processes, including
instruction/unwind, imported function and table aliases, active segments,
table mutation, and cold/warm cross-process cache paths. The host-owned Unix
server passed 8/8 processes per product. An updated, separately hashed test
runner inspected actual LLVM IR and O3 x86-64 objects under both stack policies:
the live resolver relocation appears only in debug-full, bounded typed slots
and indirect native tail jumps remain, and ordinary full executable sections
plus relocations are identical to the r247 baseline. The native EH/mmap suite
passed eight checks and ten objects per product. Its four actual mmap memory
leaf instruction-byte comparisons per product are identical to **both** r243
and r247; the unwind leaf has no helper call, guard, lock, or fence. These are
code-identity and functional checks, not a general timing guarantee.

The 724-file O3, CLI, server, IR, assembly, and codegen evidence archive is
`build/wasm3-evidence/r252-debug-full-qualified.tar.gz`, SHA-256
`5315f0a8e293eadccc34f6ba38bb382c157d10949ee67fe21931577963e226ee`;
every archived regular file was checked against an independent SHA-256 manifest.
The revised IR runner was staged after the frozen r252 source snapshot and is
identified by SHA-256 `9479d421cbf44fa7b852b0f383a9880e95e5f9cbed82438d58261429c391e755`.
The ROS r252 vendored LLVM source manifest predates the prebuilt ros.9 toolchain
used for these tests; a later snapshot must align them before claiming
source-matched ROS cross-platform qualification. General `call_ref`, retained
exception references, GC execution, complete debugger behavior, safe code/CFI
retirement, and function hot replacement remain unfinished.

## 2026-09-24 frozen Core 3 and Threads qualification

The current ordinary production source ID is
`sha256:e9873c69290b5bbbacbfe22e23543263fc619dce8bc58e54bba85fcb9b8035ad`;
the current ROS production source ID is
`sha256:41b3990b2082c50cd8482ffd3104a1c83df07572efec56fbbb0acc0cc81ccb30`.
The last source change in both repositories adds pointer ownership and diagnostic
cursor diagrams to `validation/standard/wasm3/validator.h`. The older
`wasm1p1/` and `wasm2/` validator directories remain at HEAD. The current
`wasm3/impl.h` and `impl.cppm` include and export every helper and the
standalone validator in that directory. The r26 ordinary tiered change installs
the throw-only diagnostic capture hook for T0; no normal memory handler or JIT
load is changed. ROS has no tiered mode.

Pinned WebAssembly testsuite commit `b464a4c`, wasm-tools 1.259.0, and
Wasmtime 48.0.2 were used in the SSH Linux 64 GiB/swap-0 cgroup. Each final O3
binary passed the same 33 official WAST files' binary-validity corpus:
648 modules and 1296/1296 Wasmtime/product validity verdicts per repository.
The 17 wrapped official runtime modules passed 255 reference/product runs
covering 177 numeric return assertions, including `call_ref`,
`return_call_ref`, `try_table`, `throw_ref`, GC arrays, memory64, and table64.
Eleven official `assert_exception` cases passed 165 reference/product runs;
25 official `assert_trap` cases passed 25/25 reference and 200/200 product runs.
Expanded `ref_test`, `br_on_cast`, and `ref_cast` assertions passed 223 runs
covering 90 returns, four actions, and 25 traps on the final source IDs.
The full Wasmtime WAST reference also accepted those three GC files.

These are selected official WAST cases, not an assertion that every Core 3 WAST
file ran. `type-subtyping.wast` includes multi-parent text syntax rejected by
both wasm-tools 1.259.0 and Wasmtime 48.0.2 at line 958; the binary validator
runner records one audited equivalent invalid binary instead. In three GC
files, a guest CLI cannot construct the original host `(ref.extern 0)` value;
18 assertions that depend on it were excluded, with the original module and
the selected assertion line numbers retained in the evidence manifest.

The ordinary all-combine/delay O3 VM passed GC new-syntax 597/597, exnref/GC
payload 173/173, exnref table/table64/cross-module 156/156, official
`try_table`/`throw_ref` exception 66/66, mixed T1-to-T0 stack 2/2, and
`try_table` plus shared-memory atomics under feature-switch and combine/delay
variants 75/75. The ROS all-combine O3 VM passed general GC 233/233,
exnref combinations 61/61, and canonical same-structure/different-structure
GC casts 24/24 including Wasmtime. The ordinary all-combine O3 VM passed the
canonical GC positive/negative matrix 100/100 across 49 configurations and
the two Wasmtime references. Both products' normal full/lazy/tiered
configurations are also exercised by the official runtime and cross-architecture
tests above. A rejected optimization probe proves that a single host/native
boundary is insufficient to skip native unwinding: in a real cross-module
LLVM-T1 caller to interpreter-T0 callee it omitted the T1 frame. The production
path retains the complete native walk and a two-repository regression test.

ROS final-source AArch64 and RISC-V64 QEMU VMs each passed the seven-mode
multi-memory/initializer matrix, LLVM-full cross-function exception plus
shared-memory atomic execution under instruction and unwind policies, and
canonical GC cast positive/negative cases in interpreter and JIT modes.
The ordinary final-source QEMU 51-mode matrix and Windows PE/Win11 runs were
still in progress when this section was written; their results must be added
before claiming full requested platform acceptance.

Final-source macOS arm64 builds and runs used a per-process-tree 4 GiB RSS
watchdog: ordinary peak 3,728,867,328 bytes, ROS peak 3,568,828,416 bytes.
The ordinary 10 and ROS 11 acceptance categories passed, covering C/C++/Rust
DWARF source, source/Wasm/ARM64 stepping, authenticated late FD and host-owned
server control, sealed guest access, typed GC same-ABI function hot replacement,
unwind hot replacement, Core 3 instructions, and canonical GC casts. On arm64,
`auto` selected native `unwind.h` with generated logical frames omitted; the
trapped GC fixture still reported the Wasm call stack. LLVM-full-only debugger
and function replacement correctly reject other runtime modes.

For the final ordinary Mac JIT, an actual AArch64 Wasm32 `i32.load` hot path
contains a base materialization and one `ldr`; it has no per-access guard,
lock, or branch. The mmap hardware protection page remains in use. The final
Linux O3 interpreter audit found 498 full-mmap register-ring handlers without
call, lock, or fence in their hot paths; load handlers have no branch and end
with indirect tail dispatch. A separate actual AArch64 O3 audit found 794/794
ordinary and 397/397 ROS full-mmap handlers without a helper call, lock, or
barrier; every load is branchless and every handler has `br xN` tail dispatch.
The final ROS Mac JIT memory load has the same direct `mov`/`movk`/`ldr` shape.
These code-generation checks establish the absence
of added per-access guards. They do not prove a universal timing ratio.

## 2026-09-25 Core 3 breadth rerun (r32)

This section supersedes the r26 source IDs above. The current ordinary
`src` plus `third-parties` content ID is
`sha256:939c12c5f8f84e64f3097a5ddfab5d58fec2907eecb2377af3950d6024586ff8`;
the current ROS ID is
`sha256:ac49101ff52e6fe7a599442461b7e56f7e3384c645d86f0e0b6373b033ccca68`.
Their x86-64 O3 binaries have SHA-256
`24c2c2551b36eee11866c9af5270d3261e3f16445bf7dc22a9768712a11e873d`
and `dce32293377da0ef6623e6203bcda58ed6cde6f23b5d1b64fa5426776c6f551c`,
respectively. The broad differential and principal runtime result sets record
their source IDs and binary digests; ancillary summaries use the same binaries.

Both products now match Wasmtime 48.0.2 on the binary validity of **5924/5924**
modules derived from all 257 top-level WAST scripts at pinned WebAssembly
testsuite commit `b464a4c` (also the remote `HEAD` when checked on
2026-09-25). The main conversion produced 5833 binaries from
256 scripts. `type-subtyping.wast` contains one invalid two-supertype WAT
assertion that wasm-tools 1.259.0 refuses to convert; a narrowly sanitized
copy supplied its other 90 binary modules, and an exactly equivalent manually
encoded invalid two-supertype binary supplied the 91st. The sanitizer records
both original and sanitized digests. Each product has zero verdict mismatches
on the 5833-plus-91 set. This is a validation differential, not an execution
claim for all WAST assertions.

The Core 3 corrections uncovered by that differential are synchronized across
the two repositories. Legacy funcidx element flags 0–3 retain their specified
non-nullable `(ref func)` element type; Core 3 element constant expressions may
read previously defined immutable globals. `br_if`, `br_on_null`, and
`br_on_non_null` reify the full declared rich label tuple on reachable branches
in the standalone Wasm 3 validator and the interpreter/JIT compiler validators.
The former Wasm 1.1 and Wasm 2 validator directories remain byte-identical to
repository HEAD; all 26 Wasm 3 validator headers and named modules are
included/exported by their local `impl.h` and `impl.cppm`.
A full-file cursor-comment audit of the copied-and-extended Wasm 3
`validator.h` found 70 actual `code_curr` updates; all 70 have a nearby
`[safe] ... unsafe` diagram naming the cursor. This audit is scoped to that
file and is not a proof about arbitrary pointer arithmetic elsewhere.

The final-source Linux runtime also executed 17 modules wrapped from exact
official Core 3 WAST groups, preserving each original module body and adding
an `_start` that checks 177 numeric return assertions. Families were `array`,
`call_ref`, `memory64`, `ref_cast`, `return_call_ref`, `table_size64`,
`throw_ref`, and `try_table`. Wasmtime 48 passed all 17 references; ordinary
full/lazy/verified interpreter, full/lazy JIT, and tiered variants passed
187/187 runs, while ROS full interpreter/JIT instruction/unwind passed 51/51.
Both product binaries and post-run source fingerprints match their r32 build
records. The 255-run record is an execution subset of the official WAST corpus,
not a claim that every official stateful assertion was executed.
Eleven official `try_table`/`throw_ref` `assert_exception` invocations were
also run individually: Wasmtime 11/11, ordinary backends 121/121, and ROS full
backends 33/33. Every one of the 154 product failures returned nonzero and
printed a colored `fast_io` fatal with exception payload-field count and
numbered Wasm frames with function indices. The semantic check requires the
exception and captured-at-throw stack text, not merely a nonzero exit.
Twenty-five official `assert_trap` invocations in `call_ref`,
`return_call_ref`, `try_table`, and `array` additionally passed Wasmtime
references 25/25, ordinary execution variants 275/275, and ROS full variants
75/75. Four array segment-drop cases replay the preceding `drop_segs` action
inside the same instance before probing the trap. The product check confirms a
runtime fatal and excludes the wrapper's forced `unreachable` for every other
expected trap; it does not require the human-readable trap wording to match
Wasmtime byte-for-byte.

On those same final x86-64 binaries, the ordinary `_start` branch/type runtime
matrix passed 60/60 across its full/lazy/verified interpreter, full/lazy JIT,
and tiered modes; ROS full interpreter/JIT passed 12/12. Separate branch
semantic fixtures also executed both taken and fallthrough paths of
`br_if`, `br_on_null`, and `br_on_non_null`, asserting their actual values:
ordinary 36/36 and ROS 15/15 including Wasmtime references. The new funcidx
element and defined-global execution/validation matrix passed ordinary 39/39
and ROS 15/15, including Wasmtime and negative nullability checks. Ordinary
Core 3 exception handling combined with shared-memory atomics passed 30/30
across nine execution policies and precise feature-off diagnostics; its
16-fixture i31/struct/array/cast/segment/constant-expression runtime suite
passed 213/213. All are new-syntax execution tests, not older Core 1 smoke
tests. ROS also passed 185/185 reference, interpreter-full, LLVM-full
instruction/unwind, and validator executions over 37 local exception cases.
Each product passed 78/78 independent feature-switch checks: 13 current Core 3
and Threads syntax families each parsed, validated, ran in Wasmtime, ran in
interpreter/JIT full, and failed precisely when its own UWVM switch was off.
The relaxed SIMD fixture also failed when only its base SIMD switch was off.
Wasmtime 48 accepts table64 by default without exposing a separate `-W`
table64 control; UWVM's table64 switch was tested independently. The explicit
table initializer likewise has an independent UWVM switch although Wasmtime
groups its syntax under function references.
The ordinary final-source indexed-memory matrix passed 1618 actual results
over 48 positive and 12 negative current-syntax cases in 27 oracle/validator/
interpreter/JIT/tiered configurations. Two >100-memory cases exceeded
Wasmtime's configured validator memory-count limit and are recorded as
reference-limit skips, not reference passes.

The separate ordinary r32 O3 VM with all interpreter combination and local
delay macros compiled in has source ID `sha256:939c12c5f8f84e64f3097a5ddfab5d58fec2907eecb2377af3950d6024586ff8`
and binary SHA-256
`01210f69c3d5968064d630a0a422ad854a20e9c8530f1ae135f728e80b85a93d`.
Its four combination levels, two delay states, and full/lazy/verified
interpreter modes passed branch taken/fallthrough semantics 81/81 (including
nine wasm-tools/Wasmtime oracle runs), `try_table` plus shared atomic feature
gates 75/75, exnref/GC payload 173/173, and the 16-fixture GC execution
matrix 597/597. Its indexed multi-memory matrix passed 1498/1498 across the
24 mode/combination/delay variants and 58 Wasmtime oracle cases, with the two
>100-memory reference-limit skips still listed. The build manifest records
the enabled macros, source ID,
binary digest, and 64 GiB/swap-zero cgroup limits. A separate qualification
record proves the source manifest was byte-identical before and after the
build, binds all 2424 checks by JSON digest, and records zero cgroup OOM kills.

The final-source ROS AArch64, RISC-V64, and i386 Clang/LLVM 23 cross builds each
passed 24/24 QEMU executions of eight current Core 3 modules in interpreter
full and LLVM full instruction/unwind modes, including `call_ref`, `return_call_ref`,
`try_table`, GC arrays/casts, memory64, table64, and exceptions with shared
atomics. The same three QEMU targets each passed an additional `auto`
native-unwind `call_ref` execution and a Core 3 struct-payload uncaught
exception with readable leaf/middle Wasm frames (6/6 total). Every target
selected live `unwind.h` and omitted instruction frames, including AArch64,
RISC-V64, and i386. On the final ROS x86-64 executable, independent
native-unwind checks
passed 36/36 over four Core 3 scenarios and recursive traps; `auto` selected
native unwind, with correct Wasm stack reporting and without per-call
instruction-frame emission. The final ROS mmap interpreter assembly audit
checked 249 scalar/SIMD register-ring handlers: no helper call, lock, fence,
or load bounds branch; every handler ended in indirect tail dispatch. Relative
to the earlier r29 reference, 67 instruction sequences changed and each became
shorter. These assembly comparisons do not establish a wall-clock speedup.
The final ROS i386 runtime object has 44 inline `cmpxchg8b` sequences and no
unresolved `__atomic` helper; Clang atomic-alignment warnings fell from nine
in the pre-fix object to zero. Its QEMU test source fingerprint and binary
digest match the cross-build manifest, with zero cgroup OOM kills.
The final ROS macOS arm64 executable is linked from the exact r32 source ID
above. Its 12 local CLI object compilations remained below a 4 GiB
process-tree RSS watchdog (peak 3,558,522,880 bytes), and final linking peaked
near 1.04 GiB. The final binary passed Mac acceptance for C/C++/Rust `-g`
source mapping, source/Wasm/AArch64 native stepping, late host FD/server
control and sealed guest input, typed GC same-ABI function replacement,
unwind replacement, 17 Core 3 new-syntax interpreter/JIT checks, and nine
Core 3 initializer checks. Additional Mac branch taken/fallthrough checks
passed 15/15 and element/defined-global checks passed 12/12; the runtime
test process-tree peak was
395,378,688 bytes. Its build manifest records binary SHA-256
`35bd2b3d632a71c6319f1fc382864460e4628b0c4efbc4e9e6ac7b20f57948db`.
On this arm64 target, `-Rllvm-call-stack auto` selected native `unwind.h`,
omitted per-call instruction frames, and produced colored leaf/caller Wasm
frames for an uncaught Core 3 exception.

The ordinary macOS arm64 LLVM 23 JIT-full executable is likewise linked
from its exact r32 source ID above. Its source manifest was unchanged across
the build, and its process-tree RSS peaked at 3,626,156,032 bytes under the
4 GiB watchdog. Mac acceptance passed C/C++/Rust `-g` source mapping,
source/Wasm/AArch64 native stepping, late host FD/server control, sealed
guest access, typed GC same-ABI hot replacement, unwind replacement, new
Core 3 syntax, initializers, and canonical GC instruction/unwind paths. The
additional branch taken/fallthrough cases passed 15/15 under JIT instruction
and unwind, and element/defined-global cases passed 12/12. The
binary SHA-256 is
`dc77fabe7aef99b83e1e73ddce889b9de1f3573b292247e59bc6bf816575ea7d`.
Its arm64 `auto` policy likewise selected native `unwind.h` without
instruction-frame emission and captured colored leaf/caller Wasm exception
frames.

The ordinary r32 Linux executable passed 47 interactive debug console
processes including C/C++/Rust `-g` source locations, source/Wasm stepping,
and non-full-JIT fatal diagnostics; two native single-step executions, 15
late host-owned FD controls, 12 authenticated server checks, five sealed
guest-access profiles, ten function replacement cases, and six typed GC
same-ABI/stale-generation cases also passed. The authenticated server checks
include guest-peer and wrong-capability rejection, reconnect, native-trap
detach, and guest continuation. `-Rllvm-call-stack auto` selected live native
unwind on this Linux target and omitted instruction-frame emission; the
instruction policy remained a separate selectable implementation.
The ROS r32 Linux native O3 executable also passed seven final-source
debugger suites: Core 3/EH interactive use and incompatible-mode fatal
diagnostics, native assembly stepping under instruction and unwind policies,
late host-only FD control, authenticated server reconnect and rights rejection,
same-ABI replacement with active-frame rejection, typed GC replacement, and
sealed guest input. Its source manifest was unchanged before and after the
suite; the Linux binary SHA-256 is
`dce32293377da0ef6623e6203bcda58ed6cde6f23b5d1b64fa5426776c6f551c`.
The final-source ROS MCJIT `i32.load` executable sections and relocations
match the earlier r29 memory fixture byte-for-byte under both instruction and
unwind policies; its leaf uses a direct native load without a per-access
software guard, lock, or helper. The mmap hardware protection page remains.
The ROS final-source dynamic `i32.store` JIT assembly has a single direct
`movl` on the page-interior path. Only a possible 64 KiB page crossing branches
to a volatile final-byte probe before that store; there is no helper call,
lock, CPU fence, or memory-length load. Fault-time prefix preservation still
requires the separate live store tests.
The ordinary final-source LLVM JIT mmap memory leaf, complete executable
sections, and relocations likewise match its r30 baseline byte-for-byte under
both instruction and unwind policies. An ordinary x86-64 interpreter audit
checked 498 complete mmap ring handlers: no helper call, lock, or fence; loads
have no bounds branch, and all handlers end in indirect tail dispatch. The
remaining changed store specializations are under manual codegen review;
instruction counts alone are not a safety or timing result.
The ROS final-source AArch64, RISC-V64, and i386 mmap interpreter binaries
also have source- and ELF-hash-bound disassembly samples. Their selected
`i32.load`/`i32.store` register-ring handlers use native memory instructions
and indirect tail dispatch; a possible cross-page store probes its last byte
before the actual write. No selected success path has a guard object, lock,
CPU fence, or memory helper. These cross VMs were built at `-O1`, so their
instruction counts are not presented as `-O3` performance results.

A final-source target inventory separates runnable QEMU coverage from available
tools. ROS i386, AArch64, and RISC-V64 have actual LLVM-full execution results
above. ARM32, LoongArch64, and MIPS64EL have QEMU, a Clang target, and a
sysroot, but lack matching LLVM target archives and OpenSSL target libraries
for this build. PPC64LE and SystemZ also need a Clang/LLVM build with those
backends. Their presence in QEMU is not a claim of successful JIT execution;
remaining cross-target work is pending.

The source-bound artifacts are under `build/wasm3-evidence/final-20260925/` in
each repository, including both 5833-module result sets, both 91-module
result sets, the 255-run official Core 3 runtime subset and 165-run
unhandled-exception subset, 350-run official trap subset, branch/element/EH
runtime summaries, ROS AArch64/RISC-V64/i386
and unwind results, feature-switch results, interpreter and JIT mmap codegen
audits, final ordinary and ROS Linux debugger/hot-replacement records, conversion
scripts, and the type-subtyping sanitizer manifest. The
ordinary r32 timing audit and final-source Windows PE/Win11 acceptance are
in progress. Until these finish, the r26 platform evidence above applies only
to its named older source snapshots.

An expanded, source-bound runtime sample from nine additional official Core 3
WAST files (`array_copy`, `array_fill`, `array_init_data`, `array_init_elem`,
`br_on_non_null`, `br_on_null`, `bulk64`, `i31`, `memory_grow64`) passed 21/21
Wasmtime reference executions and 294/294 product executions: 231 ordinary
full/lazy/tiered runs and 63 ROS full runs. The 21 original-module wrappers
check 219 numeric assertions and replay 23 state-changing actions in order.
Every wrapper passed wasm-tools 1.259 validation; source WAST, original module,
wrapper, product binary and runner hashes are recorded in the final evidence.
Two `br_on_cast` groups require the spec harness to supply a host `externref`,
and two `i31` groups require imported/start-module linking; this guest-only
wrapper excludes them rather than counting them as product passes. These
focused executions supplement, but do not replace, the full official Core 3
runtime assertion suite.
An independent wrapper for official `struct.wast` and `i31.wast` further checks
float results by their exact bits and checks each component of multi-value
returns. Four modules passed wasm-tools validation, 4/4 Wasmtime executions,
and 56/56 product executions across the same 14 backend/mode policies. They
cover 36 numeric assertions, including 20 float or multi-value assertions,
plus two ordered state-changing actions. This runner records the original
WAST/module, generated fixture, product binaries, and cgroup provenance.
The same bit-exact wrapper also passed four more official modules from
`return_call_ref.wast`, `array.wast`, and `memory64.wast`: 79 numeric
assertions, including 11 float results, with 4/4 Wasmtime and 56/56 product
executions. This checks actual tail-reference calls, GC array operations,
and 64-bit memory values in all selected backend/mode policies.
The official `try_table.wast` module linked to its registered tag-provider
module also passed 35 numeric assertions, including eight bit-exact float
results, in one Wasmtime and 14 product executions. This adds exception
handling result-flow coverage across full/lazy/tiered and instruction/unwind
policies without replacing the original exception module body.
Another eight official WAST files covering `array.new_data`,
`array.new_elem`, table64 indirect calls, multi-memory, memory64 fill,
`ref.as_non_null`, `ref.eq`, and `ref.test` passed 17/17 Wasmtime
references and 238/238 product executions. The wrappers check 109 numeric
assertions and replay five ordered actions. A separate `ref_test` group uses
a host-provided `externref` action that the guest-only wrapper cannot supply;
its 66 assertions are excluded. A table64 growth group similarly depends on
host `externref` values and is not counted.
The full numeric-return portion of official `memory_copy64.wast` passed:
all 4,320 source assertions across 23 original-module wrappers, with 12
ordered copy actions, 23/23 Wasmtime references, and 322/322 product runs
(253 ordinary, 69 ROS). This exercises memory64 copy results and state
across the ordinary full/lazy/tiered matrix and ROS full interpreter/JIT.
Separate trap and fault-first tests cover out-of-bounds behavior; this
numeric-return result alone is not a trap qualification.
Official `memory_init64.wast` numeric returns also passed all 126 assertions
in ten wrappers, including four state-changing initializations: 10/10
Wasmtime references and 140/140 product runs (110 ordinary, 30 ROS).
The separate official trap and live fault-first suites remain the evidence
for out-of-bounds writes and prefix preservation.
The exact 34 `assert_trap` actions in official `memory_copy64.wast` and
`memory_init64.wast` each produced the expected out-of-bounds trap in
Wasmtime (34/34) and a non-validation fatal memory-bounds diagnostic in
all 476 product runs (374 ordinary, 102 ROS). These trap invocations and
the later numeric-return assertions run in separate guest instances here;
they do not prove memory contents survive a failed copy/init in one instance.
The live fault-first harness performs that separate check.
All 443 numeric-return assertions from official `table_copy64.wast` also
passed in 29 wrappers with 18 ordered actions. Eighteen wrappers link the
WAST's registered function-provider module `a` through real Wasmtime/UWVM
module preloading. Wasmtime passed 29/29 and the products passed 406/406
runs (319 ordinary, 87 ROS). The 1,206 official trap assertions in this
generated WAST are outside this numeric-return qualification.
Official `table_init64.wast` then passed all 121 numeric-return assertions
in ten wrappers, with nine ordered initialization actions and nine linked
uses of its registered provider module `a`: Wasmtime 10/10 and the products
140/140 runs (110 ordinary, 30 ROS). The extra `gc` feature switch is needed
for a recursive type in the final official module. This is r32 source-bound
evidence; the later Windows-only source repair is tracked separately below.
An additional scoped source audit checked every `code_curr` mutation in the
Wasm 3 validator, UWVM interpreter compiler/validator, and LLVM JIT
compiler/validator headers and modules for a nearby `[safe ...] unsafe`
cursor diagram: ordinary 79/79, 106/106, 171/171; ROS 79/79, 102/102,
160/160. Its manifest stores file hashes and reports the exact search
window; the audit is a review aid for these cursor updates, not a proof
about all other pointers or memory bounds.

## 2026-09-25 Windows source repair and table64 trap expansion (r33 in progress)

The frozen r33 Windows-build ordinary content ID is
`sha256:55dd92e7b8005c25aa5b7d2ec381edb4306ed332bfcaa6ad38161dd0642edf32`;
the frozen r33 Windows-build ROS ID is
`sha256:800c0e5252af0224df50b133b7edb4f3aa7cc3526c31a3473274434fe1585dfe`.
Relative to the frozen r32 production source, each repository changes only
`src/uwvm2/uwvm/run/run.cppm`: Windows non-Cygwin sees the
`fast_io_device.h` declaration of the process-ID API, and the module now
explicitly imports `uwvm2.uwvm.cmdline.params` for the debugger control FD.
The exact Windows Clang 22 compile command for the patched module completed
successfully; a full new-source ROS PE build and Win11 VM acceptance are in
progress. All earlier r32 Linux/QEMU/Mac results retain their recorded older
source IDs and must not be relabeled as r33 results.

The ordinary new-source macOS arm64 LLVM 23 JIT-full binary has SHA-256
`0b188645d054e775da707cf99bbdb26be9c7f28786b759613be1e01acd696647`.
Its build source manifest stayed identical before and after compilation;
the process-tree RSS peak was 3,671,113,728 bytes under a 4 GiB watcher.
Thirteen acceptance categories passed, including C/C++/Rust `-g` source,
Wasm and native stepping, host-owned late FD/server control, sealed guest
input, typed GC same-ABI replacement, Core 3 new syntax and initializers,
branch/element behavior, and native-unwind colored exception stacks.
The detailed category counts, binary hash, and process peaks are in
`build/wasm3-evidence/final-20260925/ordinary-macos-winfix-55dd-summary.json`
in both repositories. ROS new-source Mac qualification remains pending.

The exact 1,206 `assert_trap` actions in official `table_copy64.wast` and 634
in `table_init64.wast` were each wrapped with their preceding numeric calls
and state-changing actions in the same guest instance. Every generated
fixture passed wasm-tools validation, and Wasmtime 48 reproduced all 1,840
specified traps without an excluded action. On the **older r32** Linux O3
binaries, ordinary and ROS `uwvm-int full` each passed all 1,840 corresponding
fatal diagnostics. LLVM JIT full also passed every action in both products
under instruction and native-unwind stack policies: 7,360 JIT runs, 11,040
full-mode product runs. The ordinary interpreter lazy, interpreter lazy with
verification, LLVM JIT lazy, and tiered lazy policies also passed all 1,840
traps in eight configurations: 14,720 further runs, or 25,760 product runs
in total. This covers both uninitialized elements and out-of-bounds
table access; the manifest keeps the original WAST, module, fixture, runner
and binary hashes. The complete reference/product manifests and compact
summary are in `build/wasm3-evidence/final-20260925/`. The current r33
binaries still need separate source-bound replay; these counts belong to the
older r32 source IDs.

## 2026-09-27 source-bound r35 qualification in progress

The current ordinary product source ID is
`sha256:13e387be6cd5b2c8e4187854ce33711c22403110932faf41857bb4499146add7`;
the ROS ID is
`sha256:193fcf80cd339df35b7db661d13f16426015cef8e961b845edf1e960d670a56e`.
These include the reviewed Windows module-consumer declaration fix in
`runtime/lib/uwvm_runtime.module.cpp`. The previous r33 ROS Windows build
reached the runtime module consumer but failed because that translation unit
had not included the Win32 declarations used by `native_step_windows.h`.
That r33 failure is not an OOM result. Its PE/Win11 qualification and both
products' r35 Linux/QEMU replays are pending.

The ordinary r35 arm64 macOS JIT-full binary has SHA-256
`3b59127f6f6aabf9a4d9e7a919661a7a5673a8322224a52bf742a2e01f7f0af6`.
Its source fingerprint was identical before and after the build; the
process-tree RSS peak was 3,695,919,104 bytes under a 4 GiB watchdog.
The source-bound macOS acceptance passed seven debugger/hot-replacement
categories, ten Core 3 new-syntax rows, six initializer rows, and four
canonical GC checks each under instruction and native-unwind stack policies.
The exact commands, output logs and per-test memory peaks are recorded in
`build/wasm3-evidence/macos-final-r35-ordinary/manifest.json` and the build
watchdog result in `build/wasm3-evidence/final-20260925/macos-r35-ordinary-build.json`.
The ROS r35 arm64 macOS full-interpreter/full-JIT binary has SHA-256
`6c098d1eef27fbfba29e490efbf6ba821b3f8017e3a6861d4d004c38ae27e9c2`.
Its 71,682,128-byte runtime object was cross-compiled from the exact ROS
source inside the Linux 64 GiB, swap-free, 4-P/16-E CPU cgroup; the object
hash is `8205ff732fc75033a0b0ec8b6764d2ad3c68293085d4ac803b948afc3cc4656b`.
Twelve remaining CLI objects and the link were watched under 4 GiB on
macOS. The source and all object hashes matched before/after linking.
The ROS acceptance passed seven debugger/hot-replacement categories plus
ten LLVM new-syntax, seven interpreter new-syntax, and nine initializer rows,
each under the macOS 4 GiB test limit. The source-bound build, cross-object
and acceptance manifests are in `build/wasm3-evidence/macos-final-r35-ros/`
and `build/wasm3-evidence/final-20260925/macos-r35-ros-runtime-cross/`.
These r35 results precede the later Core 3 exnref cast/test fix.

## 2026-09-27 Core 3 exnref GC cast/test repair (r36 in progress)

The current ordinary source ID is
`sha256:050a64a89fc1ba18dc7ea6a42757cb6c5b4ae0bd46b910e55c801c0a3a9e61e9`;
the ROS ID is
`sha256:4172463c125f85789554209573501df1b56fac441fd7d899e84c599fbb5a15d8`.
Official wasm-tools and Wasmtime 48 accept and execute
`ref.null exn; ref.test (ref null exn)`, and the interpreter executes it.
The r35 ordinary macOS JIT and older Linux ordinary/ROS JIT binaries instead
failed materialization: the shared GC reference-test emitter accepted only
function and external reference carriers. The r36 fix admits the Core 3
exnref's complete 16-byte tagged carrier while retaining the width check.
The same helper serves `ref.test`, `ref.cast`, and `br_on_cast`. New
source fixtures and a mode/feature-gate differential runner are in
`test/0017.runtime/`. Linux O3, macOS and Windows r36 source-bound
requalification remain in progress; older r35 binaries are baseline
evidence, not validation of the repair.

The r36 ordinary and ROS Linux O3 products have SHA-256 values
`13978447a9d36f5e91ca342b131929935debaed90201c2cf582361d12c68695a`
and `6346d400963942567e8479e30c091a1360facaa689a0fa12d0763baa75d657d3`.
The new five-fixture differential suite passed 90/90 ordinary and 50/50 ROS
checks against wasm-tools and Wasmtime 48. It covers interpreter full/lazy,
LLVM full/lazy, tiered T0/T1, both JIT stack policies, precise GC/exception/
reference-type feature gates, a retained nonnull exnref, and a required
null-to-nonnull cast trap. The pre-fix products passed only 60/90 and 40/50;
their failures were confined to JIT materialization. Evidence with source,
binary, fixture, command and output hashes is in
`build/wasm3-evidence/core3-gap-audit/{ordinary-r36-formal,ros-r36-formal}/`.
The ordinary r36 arm64 macOS JIT-full binary SHA-256 is
`96e547e9003da2469d4fd7f0e89072a860c6c9a4c190be48fc52fb4f87d7b771`.
It built under a 3,682,189,312-byte process-tree RSS peak and passed the
existing debugger/hot-replacement/new-syntax suite plus ten new exnref
cast/test executions under instruction and unwind; logs and hashes are in
`build/wasm3-evidence/macos-final-r36-ordinary/`.

The r36 ROS arm64 macOS interpreter/full-JIT product SHA-256 is
`df7b05abb0bfd2683c9a068e27ddc42f0c96fda55be51160d861bd216c9b3e55`.
Its 71,682,192-byte runtime object was cross-compiled from the exact ROS
source in the Linux 64 GiB, swap-free, 4-P/16-E CPU cgroup, with OOM counters
remaining zero; object SHA-256 is
`345842088e2aba6dc57d532291d45b673b68f107b1e5eb0d53e028ec40209867`.
The remaining 12 CLI objects and link were watched on macOS, with an overall
process-tree RSS peak of 3,516,514,304 bytes under the 4 GiB cap. The
source and object hashes matched before and after linking. Seven debugger
and hot-replacement categories and 26 Core 3 rows passed. Another 15
exnref cast/test runs passed in interpreter full and JIT full with both
instruction and native-unwind stack policies. Build, qualification and
new-syntax logs/hashes are in `build/wasm3-evidence/macos-final-r36-ros/`;
the Linux cross-object provenance is in
`build/wasm3-evidence/final-20260925/macos-r36-ros-runtime-cross/`.
Windows PE/Win11 qualification remains pending.

## 2026-09-27 Core 3 exnref function ABI repair (r37 in progress)

The current ordinary product source ID is
`sha256:87c976b2b5e40b9d572f1dd1c09e6f07116736edde1145d3b0695dbe0f521851`;
the ROS ID is
`sha256:8c261ac4c4c2d15cbd49a23404982382b8722a0490169d6a6baace3c9ac3659b`.
The [Core 3 text syntax](https://webassembly.github.io/spec/core/text/types.html#text-reftype)
defines `exnref` as `(ref null exn)`. A valid function type with an `exnref`
parameter or result passed pure validation and LLVM JIT on r36 but caused
the interpreter to terminate even when the function was never called.
The interpreter's per-function call metadata computed a zero ABI byte width
for the unnamed legacy-enum carrier `0x69`. Both repositories now assign it
the complete `sizeof(wasm_externref_t)` tagged-reference footprint, matching
the existing local and operand-stack layout. No pure validator or memory hot
path changed. Ten formal modules now cover GC cast/test, null and retained
exception references, unused exnref signatures, direct and indirect calls,
and exact feature-off diagnostics. On r36 the expanded suite passed only
125/180 ordinary and 85/100 ROS checks; the five new ABI modules caused all
failures. On fresh source-bound Linux O3 r37 binaries it passed 180/180
ordinary and 100/100 ROS, including all requested full/lazy/tiered policies
available in each product and both JIT stack policies. Binary SHA-256 values
are `e5810b0fb85e660329d864af034e0a5b66e6f010c3eb27b35a3d5b16c19ac505`
ordinary and `0110ce10a83c8db611f1857632c6025458cc762930046b9c753afc19e50760d9`
ROS. Formal fixture, source, binary and run hashes are in
`build/wasm3-evidence/core3-gap-audit/`.

The r37 ordinary arm64 macOS JIT-full binary SHA-256 is
`a4d1193fa754fe8a4f98c29b8a92fc65f075b4eca5d1a55c6c9a3d046926312c`.
Its build stayed below the 4 GiB process-tree cap at 3,686,039,552 bytes;
seven debugger/hot-replacement categories, the existing Core 3 syntax and
initializer rows, canonical GC under both stack policies, and 20 new exnref
executions passed. The r37 ROS arm64 macOS interpreter/full-JIT binary SHA-256
is `5a16a2d610246443c94bc670cedbae508da1c75f47d785f9843bd60f147a6d7e`.
Its source-matched runtime object SHA-256 is
`4635d89859d9042cda46883688baacc2f19fd7137033cd0a1447c0de91d95dfe`;
Linux cross-compilation used the 64 GiB, swap-free, 4-P/16-E cgroup with
zero OOM events, while the 12 CLI objects and link stayed below 4 GiB on
macOS (3,561,291,776-byte peak). Seven debugger/hot-replacement categories,
26 Core 3 rows and 30 new exnref executions passed in interpreter full and
JIT full with instruction/unwind. Evidence is in
`build/wasm3-evidence/macos-final-r37-{ordinary,ros}/` and
`build/wasm3-evidence/final-20260925/macos-r37-ros-runtime-cross/`.

On the r37 Linux O3 binaries, the mmap interpreter load and fused-load
hot-path assembly matched r36 byte/instruction counts, with tail dispatch
and no extra per-access branch, call, lock or fence. Actual product JIT guest
objects for the memory fixture retained three semantic reads/writes;
instruction-mode normal path had 16 instructions/two calls and native-unwind
eight instructions/zero calls, with no lock or barrier. Both products passed
uncaught Core 3 exception diagnostics for struct and exn payloads in
interpreter/JIT and both explicit JIT stack policies, including colored
fatal output and the throwing Wasm stack. Final r37 fault-first, strict
combine/delay ABI, quiet-cgroup EH/thread performance, QEMU and Windows
PE/Win11 qualification are still pending and must not be inferred from r36.

## 2026-09-27 r38 source-bound Linux and macOS qualification

The current ordinary source ID is
`sha256:4899ea45589718b2452101022b6d6103c74a0ef36c606a44f0701089843f3726`;
the ROS ID is
`sha256:9a56c59373f8f34efe1da1b50312bb324088c37a2f791b42a8217aeaea54926c`.
Relative to r37, the production change adds five nearby pointer-boundary diagrams
for interpreter LEB and signature-iterator advances. Independent review checked
their bounds and verified that both repositories' older pure Wasm 1.1 and Wasm 2
validator directories remain identical to HEAD. All 26 Wasm 3 header/module
pairs are covered by `wasm3/impl.h` and `impl.cppm`.

Fresh Linux O3 products built from these exact source IDs in the 64 GiB,
swap-free, 4-P/16-E cgroup with zero OOM events. Ordinary binary SHA-256 is
`235d88f8e05618c4a1215df29d60743fa9d548f6ffb65cf409fa6114a2ff38e2`;
ROS is `74903aa2cd1197626aab88475992e863cc37f465a013be64d2e6922681aa223f`.
The ten newest Core 3 GC/exnref-cast and function-ABI modules passed
180/180 ordinary checks across interpreter full/lazy, JIT full/lazy and
tiered T0/T1 with instruction/unwind policies, and 100/100 ROS checks across
interpreter full and JIT full with both policies. Each suite includes
wasm-tools parsing/validation, Wasmtime 48 execution, and four independent
feature-gate checks per module. Real-product mmap interpreter and LLVM JIT
guest assembly retained the r37 successful memory path, without a new
per-access guard, lock or fence. Both products passed colored uncaught
Core 3 struct/exn diagnostics in interpreter/JIT and explicit JIT
instruction/unwind policies. Hashes and logs are in
`build/wasm3-evidence/final-20260925/r38-linux-compact/`.

The ordinary arm64 macOS JIT-full product SHA-256 is
`75d517dd7dc70e94805b5b9afd59fc0bd7de0ea0472818b6b59bd6c1c8c9ca7a`.
It built at a 3,728,441,344-byte peak below the 4 GiB process-tree cap.
Seven debugger/hot-replacement categories, ten earlier Core 3 syntax rows,
six initializer rows, canonical GC under both JIT stack policies, and
20 new exnref executions passed. The ROS arm64 macOS product SHA-256 is
`bd622b232d4adb0842e976e0549d62f268f6f2b02eb72434f57b7d6503e8bdfb`.
Its source-matched runtime object was compiled in the Linux cgroup; all 12
remaining CLI objects and the link stayed below 4 GiB on macOS, with a
3,513,761,792-byte maximum. Seven debugger/hot-replacement categories,
26 Core 3 rows and 30 new exnref interpreter/JIT executions passed.
The before/after source IDs were unchanged. Reproduction logs and product
hashes are in `build/wasm3-evidence/macos-final-r38-{ordinary,ros}/`.

Windows PE/Win11, the full r38 fault-first and strict combine/delay suites,
quiet-cgroup EH/thread performance, and QEMU execution remain separate
qualification gates. A code audit found that native cross-function guest
exception handling in the LLVM JIT currently requires Itanium/DWARF and is
disabled on Windows; Windows JIT EH cannot be declared complete merely from
a successful PE build. A valid cross-function `throw`/`try_table` Win11
runtime probe is required before closing that gap.

## 2026-10-01 fused-validation r7 qualification and r8 subtype repair

The fresh Linux O3 r7 ordinary source ID is
`sha256:13612d7ede3941f21f360e58c62fcfeffba1b6415937e65ce013eea23c217a8e`;
ROS is `sha256:d980ad60ac5e91b02c4cb1f52ae4c7982a5352c06827a8c7215903a83c2f117b`.
Both products built in the 64 GiB, swap-free, 4-P/16-E cgroup with zero OOM
events. Binary SHA-256 values are
`915d04070fb4f3ec325258f57522cf40247b2a0a5cbdbba920ce3842a743e144`
ordinary and `fa994182e9545c8acbe6e10806811580a937158e6de1de9c1be8db909291105b`
ROS. Before/after source fingerprints include bundled dependencies.

The 78-case fused-validation suite passed 606/606 ordinary checks
(450 product entries) and 426/426 ROS checks (270 product entries). This
includes the newest definite-local-assignment, nullable bottom heap,
function/aggregate type-kind, exception-reference, numeric exception-plan,
feature-gate and padded-u64 memarg cases. Pure validation and the requested
full/lazy compiler entries consume the same official-validated binary.
Verbose JIT receipts confirm actual optimized/finalized native code for the
numeric exception and abstract-reference fixtures, without a body fallback.
Four representative mmap interpreter load paths retain tail dispatch and
match the preceding ROS assembly exactly. This is a bounded assembly check;
it does not establish every combine/delay configuration or a timing gain.

Sixteen cold GC/EH receipts passed under both instruction and native-unwind
policies on both products. The 16M-allocation GC workload reported 3906
collections and 15,997,951 reclaimed slots. These receipts explicitly have
`timed=false`; they are functional evidence, not accepted P-core performance
samples. The benchmark plan binds current source, ELF and fresh O3 commands.
Formal timing still requires an independently qualified quiet, cool window.

Eight additional declared-function-subtype fixtures exposed a separate r7
execution defect despite successful official and pure validation. Successful
cases cover indirect/ref calls, tail calls, result covariance and parameter
contravariance. An unrelated final function with identical machine carriers
must still trap. The r8 repair uses actual-to-expected defined-type matching,
caller-forest integer intervals in generated JIT checks and a bounded TLS
cache of successful cross-module type projections. Executable entry addresses
remain freshly resolved. Initializer function imports use the same direction;
tag identity and replacement ABI identity retain exact comparison. Native
object-cache policy advances to runtime ABI v27 for the new type encoding.

The frozen r8 source IDs are
`sha256:d2688f9afdd23bb95937b3bd1314fdf2266dc32e55f527c7b570be9255d14919`
ordinary and `sha256:2f7d03ed7352a28a6a25eec04a27683a5c599782d6368bc06562ebc582062017`
ROS. At this entry's publication the fresh Linux builds are in progress;
the 86-case suite and nine new cross-module import/table/ref cases are
prepared but not qualified on r8. Current Windows EH, final-source QEMU,
macOS, all combine/delay and accepted GC/EH/thread timing remain open gates.
Neither r7 nor pending r8 establishes complete task acceptance.

## 2026-10-01 r8 regression closure and r9 imported-function identity repair

The frozen r8 builds completed successfully. The original 86-case suite passed
662 ordinary and 466 ROS checks; an independently frozen eight-case extension
passed another 56 ordinary and 40 ROS checks. The nine cross-module fixtures
passed 117 ordinary checks in eight supported modes and 63 ROS checks.
Tiered plus full compilation is an unsupported CLI configuration and is not
counted as a Wasm semantic result.

Two additional, officially validated fixtures then reproduced an r8 execution
failure: a consumer imports a provider's child function as its parent, stores
that imported reference in a table, and calls it as the actual child type.
All 16 ordinary and four ROS product entries incorrectly trapped. The retained
failed records distinguish this real defect from the earlier passing suites.
They are mirrored with successful receipts and the unaccepted timing data in
`build/wasm3-evidence/fused-inline-20261001-r8-compact/`. The archive SHA-256 is
`21d6fe1531ae6b57ee7b92a1d66a37d8f8868b691d9a0314217c73b28a2fa3f5`.

The r9 repair resolves imported aliases to their authenticated defined provider
before checking the actual function type. It retains the original import's
reference namespace and host import contract. Initializer admission follows
bounded, membership-checked re-export chains. GC function references now match
defined function heaps through an immutable callback bound at store creation;
both compact and ordinary representations use that same matcher. These changes
preserve exact tag and replacement ABI identity. Normal compiler entries still
validate and translate together; no whole-body prevalidation pass was added.

Fresh O3 Linux r9 source IDs are
`sha256:5f433a669066a9a35401648b7c76d87745692ca0e507455e5e07951e648cb1e5`
ordinary and
`sha256:c680525917601070275ff61b7cf5fd90b5c5e07d4b196451c6dba4a9679d987a`
ROS. Fresh binary SHA-256 values are
`17f156ce090788a3a0306ab9d5808c9db501f14552bbe631a4507d2ebe264b74`
ordinary and
`a5336b707f785116a56c4473d3dd289853d0893d051f1bc1c9213e51b1eff8ad`
ROS. Both runtime and CLI were rebuilt, with unchanged before/after source
identities, in the exact 64 GiB, swap-free cgroup.

The expanded 97-case suite passed 957 ordinary checks (763 product entries)
and 521 ROS checks (327 product entries), including typed function GC casts
and constexpr struct/array fields. The 17-case cross-module suite passed
229 ordinary checks and 127 ROS checks, including re-export admission,
import-as-parent table calls, tail calls and GC casts to the real child.
These are functional receipts, not final platform or performance acceptance.

The r8 P-core pilot actually executed 90 samples but accepted zero. Thermal
and duration constraints rejected every pair; peak temperature reached 99.05 C.
Its raw timings must not be presented as a speedup. Independent long-workload
development diagnostics retain failed quality conditions and explicitly set
`formal_acceptance=false`; they will inform optimization without replacing
the qualified benchmark guard. Accepted current GC/EH/thread timing, Windows
multi-object native EH, current-source QEMU/macOS and the remaining complete
configuration matrix are still open.

## 2026-10-02 performance policy and long-workload receipts

The r9 compact build, regression and 38-check cold workload evidence is now
mirrored in both repositories at
`build/wasm3-evidence/fused-inline-20261001-r9-compact/`. Its archive SHA-256 is
`9c985a48dcd9f2b7a47e80ebf0bb7018aa7bfe97fb577eb6b36dbaff6129c987`.
Both products completed 128M and 512M GC allocation workloads under instruction
and unwind policies, with exact checksums, 31,250/125,000 collections,
127,998,975/511,998,975 reclaimed slots and zero remaining issued objects.
The sealed path reported one retirement per complete workload, not one per
iteration. These cold receipts were on E-core 16 and are not P-core rankings.

The four representative mmap load/store assembly paths in both products were
also checked against r7: eight comparisons preserve the complete normalized
instruction sequence and indirect tail dispatch. This bounds the claim to
the four selected operations and does not qualify every configuration.

The user's new benchmark criterion records temperature without using it to
reject a sample. New measurements focus on actual frequency, workload identity,
process ownership and cgroup limits. Earlier temperature-rejected records
remain historical; their temperature thresholds do not govern the new runs.
Intel VTune CLI Hotspots and `uarch-exploration` collection use the official
Intel 2026 command-line manual and installed-tool help. Profiling samples
are kept separate from unprofiled throughput comparisons.

## 2026-10-02 r10 validation repair and hardware profiling

The immutable r9 executable reproduced four valid memory64 stack-polymorphic
cases rejected by LLVM full/lazy compilation. Pure validation and interpreter
compilation admitted them. The known-i32 negative controls remained invalid.
The r10 fix uses the existing shared Core 3 operand matching rule in LLVM
memory.grow and bulk-memory validation, during translation. It adds no separate
whole-body validation pass and no runtime memory-access check.

Fresh O3 runtime and CLI builds in the 64 GiB, swap-zero Linux cgroup passed the
105-case fused corpus: ordinary 1,029 checks, including 819 product entries;
ROS 561 checks, including 351 product entries. The 17-case cross-module corpus
passed 229/127 checks. The eight new memory64 priority cases, GC function-type
and imported-alias priority suites also passed. Both source identities remained
unchanged through compilation; no old runtime object was reused.

| Frozen Linux build | Source identity | Executable SHA-256 |
| --- | --- | --- |
| Ordinary r10 | `sha256:2acdc68e84a815f2bb8e046902f3170d90e0dc60fd999fa669e99cce74f47c2c` | `abd56e284f7fd5af0f118a835c2b60bda81cc66390f4a0d32d75e4dee9a2bb7f` |
| ROS r10 | `sha256:a841ae794f515d64f545026699000fc2a74ef97eaf507732aed5d3518a5fbdb8` | `dc50a217e76b2d800e8ae16cee94b5521a52956a1a0a692c47ee51399ccfe142` |

The [r10 evidence mirror](../build/wasm3-evidence/fused-inline-20261002-r10-compact/mirror-manifest.json)
is identical in both repositories. Its 3,053 payload files total 23,332,494
bytes, including the archive. The archive SHA-256 is
`d16a9d5f36999a20cfc94675f60b342be18323155e7c41a21e6377c56266bcbb`;
the mirror manifest SHA-256 is
`1370f5216f480c8c0fb2abb95ce09f3ae873e6306f66ab4785da9858d8717fdf`.
Build receipts, before/after source manifests, actual command arrays, all five
suites' rows and source/executable identities were independently checked before
and after copying. This qualifies the named Linux suites, not complete Core 3
conformance, every interpreter configuration or every platform.

After the user set perf_event_paranoid to zero, both products completed actual
VTune hardware Hotspots and Microarchitecture Exploration collections using
driverless Perf. Subsequent profiling uses hardware counters; failed earlier
software attempts remain historical records. The microarchitecture reports'
MUX Reliability is about 0.56, so their event ratios require qualification.
Native EH callstacks show both diagnostic _Unwind_Backtrace and native
_Unwind_RaiseException paths. Their first 1,024-byte stack samples contain
skipped frames and do not establish complete phase percentages. The
[VTune analysis](../benchmark/0004.wasm3-core/VTUNE_CLI_20261002.md) records the
actual collections, ownership, limits and attribution evidence.

Linux subsequently restarted. Its recorded boot identity changed; the old
ordinary 4,096-byte collection completed, while the interrupted ROS collection
is not a pass. The keeper verified perf_event_paranoid remained zero and the
toolchain and immutable r10 artifacts persisted. No administrator command was
needed for that recovery. If a later actual environment failure needs elevated
permissions, provide the user with the specific command; the user performs it.

The performance protocol now distinguishes unprofiled guest/system time,
VTune hardware sampling and independent hardware counting without sampling.
Each needs its own actual workload, source/executable hashes, frequency and
counter runtime evidence. None substitutes for another's timing or ROI.

Current development prepares a separate r11 collector optimization that reuses
an authenticated compact slot and combines root admission with marking inside
the closed collection interval. It retains whole-graph validation, ownership,
stopped-mutator requirements and failure-before-reclamation behavior. r10
functional results do not qualify that newer source. Windows multi-object EH,
current-source QEMU/macOS, debugger/hot replacement and the complete final
configuration/performance matrix remain open.

## 2026-10-02 collector candidate, native object and platform follow-up

The r11 minimal collector candidate is now implemented and statically reviewed in both repositories. Successful root admission is performed by the existing mark loop. The work-allocation failure path retains the original invalid-root-before-OOM priority. The collector uses the private bounded bitmap helper with the exact descriptor/slot pair just authenticated in the same closed pause. Whole-heap/exn validation, canonical ownership, cohort closure, epoch commit and no-reclamation-on-failure remain intact. The two reconstructed before-images exactly reproduce the r10 header hashes.

The candidate GC headers are `55cb0d40fb264ba9b825f551d2de16c1f79b0cc9db2eb08de092651ef29ee036` and `9a770293c14aa2fd7406af8dff5b4b3c0d6d34a4f9511b3f0b2b546872dffb62`. New component counterexamples cover duplicate/foreign roots, late forged and stale roots, OOM precedence, unchanged epochs and retained object readbacks. This remains an uncompiled/unbenchmarked candidate. The [safety design](../test/0017.runtime/wasm3_gc_collection_r11_design_20261002.md) states its authority and failure guarantees.

Both r10 products actually exported the finalized self-checking 512-million-step GC JIT object. The 5,472-byte objects are identical, SHA-256 `efbcd8fa773a3bf489608053f6e47547244255aa186de36ab8bd0264adb1021e`. The observed complete successful loop has no per-iteration native call or locked/fence instruction, while retaining entry, epoch, table extent, token kind/range/frontier/live checks. Refills, collection and failure edges retain their calls. This is a cold emitted-code witness, not a timing comparison. The [native-loop audit](../test/0017.runtime/llvm_gc_native_loop_audit_20261002.md) records actual offsets and the limits of future check reuse.

Supplementary native EH runs completed 44 checks per product. The four recorded failures per product came from a test requiring an optional single-source optimization log for preloaded tag aliases. Actual exception payloads and original caller chains were correct. The corrected supplementary runner requires actual full-only native translation and completed materialization for all participating modules, and records optional source ownership separately. SHA-verified raw-log reanalysis passed; fresh corrected executions remain pending. [Original raw failures, Windows build errors and GC objects](../build/wasm3-evidence/platform-diagnostics-20261002-r10-r1/mirror-manifest.json) are preserved in both repositories: 219 files, 7,273,640 bytes, manifest SHA-256 `238adbe0199e3dd4ed3c25505994c22cde543184abf914b0b38fa040b0783512`.

Fresh Windows compilation exposed SDK macro collisions with LLVM COFF enum names. A Windows-only wrapper temporarily isolates/restores the complete bundled COFF IMAGE_* vocabulary, copies four actual enum constants to nonmacro aliases, and is used by both the ordinary header and module global fragment. It changes no vendor LLVM, executable image policy, memory hot path or unwind registration contract. Independent r10-plus-fix Windows source receipts and fresh build/VM qualification are required; the immutable r10 Linux snapshots remain unchanged.

Independent perf-stat counting failed inside the Docker namespace at perf_event_open with EPERM before its enable ACK. The owned guest remained stopped and was safely retired. Docker's actual seccomp mode is active, with no extra capabilities; host VTune hardware collection in the same constrained cgroup previously worked. The failure is not a hardware-counter pass or justification for software fallback. A separate host-namespace counting runner/guard is being prepared for that same actual 64 GiB cgroup; the old frozen container guard and runner remain unchanged. No administrator command, global security change or container capability change has been performed.

The corrected supplementary native-EH runner subsequently completed fresh Linux full-only execution: 44/44 checks for ordinary and 44/44 for ROS, including both instruction/unwind policies and official wasm-tools/Wasmtime outcomes. All original payload/frame and per-module native materialization proofs passed; source/executable/runner and before/after immutable hashes remained closed. The [fresh r2 mirror](../build/wasm3-evidence/native-eh-trace-20261002-r10-r2/mirror-manifest.json) contains 194 files, 548,754 bytes, manifest SHA-256 `a1c78181d848a9da8968f0814b7f94fc92dbf2ecbd758a68a3b20f79e07841cb`. Old r1 failures remain separately preserved. This new result qualifies these full-only Linux traces, not lazy, Windows, macOS, QEMU or EH latency.

Windows compilation passed the COFF macro collision after the isolated fix, then exposed the POSIX-only diagnostic capture macro mistakenly enabled in the Windows build recipe. Both runtime and CLI recipes now omit that test macro, explicitly recording capture_compile_enabled=false; all six candidate feature macros stay unchanged. Source identity is unchanged by this recipe correction. Fresh recompilation and VM execution remain pending. KVM access in the same Docker-owned cgroup was denied by the device policy; the previously established same-cgroup Windows TCG route is used without changing global privileges or device access.

## 2026-10-02 additional widths, lazy traces and controlled recovery

Fresh ordinary native-EH trace execution passed 80/80 checks across interpreter
and LLVM full/lazy/lazy+verification. The 24 lazy JIT runtime rows prove actual
successful compilation, with original payload/caller-chain checks under both
instruction/unwind policies. ROS full remains covered by its fresh 44/44 run.
The [ordinary r3 evidence](../build/wasm3-evidence/native-eh-trace-20261002-r10-r3-ordinary-all-modes/mirror-manifest.json)
was independently checked and mirrored to both repositories. This is semantic
trace qualification, not exception latency or tier-promotion evidence.

A separate 21-case table64 suite passed 311 ordinary and 137 ROS checks. It
uses the same official wasm-tools/Wasmtime-validated bytes to test mixed
32/64-bit table copies, overlap, table.init destination versus i32 segment
source/count, stack-polymorphic Bot versus known-wrong i32 operands, and
independent table64 enable/disable while memory64/GC/EH/threads are disabled.
The ordinary tiered rows cover entry semantics only. Root independently
regenerated the WAT text and checked every raw log, input SHA, mode/feature
argv, immutable before/after closure and resource receipt.
The [table64 r2 evidence](../build/wasm3-evidence/table64-widths-20261002-r10-r2/mirror-manifest.json)
has 984 files, 576,671 bytes, manifest SHA-256
`312f86766fd9a6a42de4bafd877e12ec4a7e66e5c4568066e9ec5c4e0056a467`.

The Windows build supervisor incorrectly applied the profiling memory.current
limit to reclaimable compiler file cache. Its owned orphan compilers were
identified and retired without changing global limits or signaling unrelated
processes. The new build-only supervisor retains kernel 64 GiB/swap zero,
a separate aggregate-owned-RSS budget, bounded admission/deadline/logs, and
PIDFD/birth/parent authentication. Actual early-bootstrap-exit and live-fork-
subtree failure controls both failed as intended, were fully reaped and left
only the authenticated cgroup init with unchanged zero OOM counters. These
are cleanup controls, not successful product builds. Fresh controlled Windows
compilation and VM qualification remain pending.

Private native-EH leaf-effect recording is now a standalone source component
in both repositories. It consumes observations from the future existing fused
pass, preserves actual linked-tag identity and ordered first-handler matching,
and permanently invalidates incomplete/allocation-failed records. It is not
imported by production, and does not yet bind canonical source/validation,
clone ABI, cache identity or loaded ranges. Its [stage-one scope](../test/0017.runtime/llvm_native_eh_private_leaf_effect_stage1_20261002.md)
remains uncompiled and unbenchmarked. GC r11 and final platform/performance
acceptance remain open.


## 2026-10-02: current counter, control and replacement work

The frozen r9 host pure-hardware run completed four source-bound rows: native EH
and the 512M-allocation GC ring for both products. Each actual grouped P0
cycles/instructions interval had equal enabled/running time (100%), completed
semantic checks and before/after source closure. The independent root review
checked raw-log hashes, ACK/GO ordering, bound PIDFD SIGINT and retirement, and
count/CPI arithmetic. The [r3 mirror](../build/wasm3-evidence/performance-20261002-host-counter-r3/curated-mirror-manifest.json)
contains 32 files, 8,230,480 bytes; manifest SHA-256
`397a28100a6b3777f5decf64ba10add8ab6d4600349f543cc12a57afbaa7a542`.
These are whole-guest counters including startup/JIT, one sample per cell,
not collector-only ROI, candidate-r11 results or an industry performance ranking.
Unprofiled elapsed time, pure counting and VTune sampling remain separate.

Shared control framing/session/launch and debugger command/controller/console
interfaces now use the ordinary repository's fast_io implementation in ROS too.
The reduced ROS compiler modes remain intact; its run integration changes only
two wire-byte routing assignments. Complete header/suffix/payload cursor safety
comments were added. Focused tests now use fast_io formatting/scanning and
assembly-linked noexcept POSIX aliases. The Linux ioctl error fixture replaces
only the later `ioctl_noexcept` call token after loading the real ABI declarations;
it asserts actual injected attempts and keeps libc/sanitizer calls outside its
hook. Compilation, sanitizer, no-exceptions and named-module replays of these
new synchronized sources remain pending.

Replacement preparation now validates and emits private IR in one fused body
pass, as normal LLVM full compilation does. Its copied body and rich locals,
original exact signature/features, owner/generation/epoch checks and publication
remain unchanged. Precise root emission explicitly uses the existing full
record's actual flag; the pending numeric plan remains null and this grants no
managed-GC admission while debugging. New fixtures cover initialized versus
uninitialized non-null GC locals and an illegal opcode after a valid GC prefix.
The runner checks rejection leaves the old result and generation unchanged,
then permits a valid replacement and rejects stale generation. This patch and
the DAP native-stop display fix have only static review so far; fresh current
product execution is still required.

The controlled Windows r2 outer build failed at an owned compiler exit-observation
transition; all bound PIDFDs retired/reaped, only the sandbox init remained, and
OOM counters stayed zero. The main compiler stage itself was actually reaped
with exit zero and its object retained; this is not a CLI or VM PASS. A restricted
exit-transition observation patch and positive/negative fault controls are being
qualified before resuming fresh host-API compilation/linking. Reuse of that
same-task main object requires current source, compiler argv and dependency
closure to match. No administrator or global security-policy change is required.

The new compact collector component declares an eligible immutable i32 field,
proves actual compact readers, uses real exclusive collection admission and
checks every surviving root and final reclamation against an independent LCG
oracle. It separates allocation, collector, readback and teardown regions; it
cannot qualify VM root enumeration or JIT code. The production r11 comparison
continues to isolate only its two GC header changes from the control/replacement
work. Both candidate compilation and paired measurements remain pending.


The four Windows build-supervisor fault controls have now been independently
reviewed against actual process identities, raw logs, retirement and OOM
receipts: early exit, descendant failure, bounded missing-exe retirement and
live-task timeout all behaved as specified. These are harness controls, not
product tests. The fresh ROS host-API object compiled successfully. Linking
then failed because the old LLVM generated configuration advertises three
nonexistent short stack-helper symbols, its integer helper references need
the actual target COFF compiler-rt archive, and OpenSSL needs crypt32 imports.
The original SDK and failed link logs remain unchanged. A private one-member
LLVMSupport repair was source-reviewed and is queued after ordinary fresh
compilation; real PE linking and Windows execution are still pending.

The future bundled-LLVM configuration patch now performs executable link
checks for all eighteen explicit compiler helpers in a function-local scope,
using independent cache keys. It does not infer the stack-helper ABI from
64-bit pointer size, overwrite the caller's STATIC_LIBRARY cross-toolchain
policy, or execute target binaries. ROS carries the source change; ordinary
carries the matching toolchain patch. This source-only change is excluded from
the already frozen Windows and two-header GC benchmark candidates. See
[CMake target-type semantics](https://cmake.org/cmake/help/latest/variable/CMAKE_TRY_COMPILE_TARGET_TYPE.html)
and [link-probe behavior](https://cmake.org/cmake/help/latest/module/CheckFunctionExists.html).

Single-module `-m debug-jit` now selects the same genuine parsed and initialized
source owner as ordinary LLVM full execution, subject to the existing full/LLVM
and no-preload constraints. This provides lifetime, not completed validation
or permission to read source variables. POSIX console flush/exit use the actual
assembly-linked noexcept ABI; Windows debugger termination uses fast_io's
nonthrowing ExitProcess declaration. These new sources require a fresh current
product replay; earlier Windows and GC results do not qualify them. Embedded
DWARF variable/inline metadata remains a separately bounded Stage1 component
under development, with external debug-file loading and runtime evaluation
excluded until a real stop/source/code-generation binding exists.

The four-build pure-hardware GC ABBA wrapper was source-reviewed and passed
65 synthetic input/build/dependency/source-difference rejection controls,
three exact-GC counter checks and frequency/count pairing checks. It has not
run. It requires real r10 A and fresh B source/build/cold receipts; only the two
reviewed GC header images may differ. The planned VTune microarchitecture
samples remain a separate measurement family; report time filters and task
labels do not establish collector-only sampling. All comparisons retain
unprofiled guest/wall timings, raw counter counts and actual frequency evidence.

The private Windows SDK repair has now compiled and closed its actual input,
tool and dependency hashes. Exactly one of the 180 LLVMSupport archive members
changed: DynamicLibrary no longer trusts the three false-positive short helper
macros. Real COFF compiler-rt and crypt32 inputs supply the remaining required
symbols; no ABI alias was invented, and the original SDK remains unchanged.
The ROS candidate linked successfully and passed its actual PE import check;
its schema-2 Windows EH stage is prepared. This is not Windows execution or
final product qualification. The ordinary candidate's two fresh exact-O3 main
compiles crossed the owned RSS limits (56 and 60 GiB), retired completely and
left OOM/oom-kill counters at zero. Neither incomplete main object is reusable.
The hard sandbox limit remains 64 GiB, with no swap. A tiny compile-only
front-end/bitcode/backend O3 study is being qualified before any full retry.

A shared wasm3 declaration-policy omission was fixed in both repositories:
exception-reference parameters/results/locals must respect a stricter exception
feature policy even when the module has no tag or throw instruction. Tests
include bare exnref, nullable noexn, non-null exn and zero-count local runs, with
separate exception/reference-type gates and function-reference independence.
The normal paths still validate and translate in one fused body pass. The
legacy wasm1p1/wasm2 validator directories remain unchanged. These new focused
component and official syntax-oracle replays are pending on Linux.

The bounded embedded DWARF Stage1 source now includes C, C++ and Rust fixtures,
DWARF4/5, optimized inline metadata and local-location descriptions. Remote
LLVM verification and native component tests remain pending. Production
metadata-only inline display is a separate source candidate: its runtime
binding requires the real initialized source, successful fused full validation,
resolved native code owner, execution lease, current function generation and
one authenticated stopped participant. It grants no local/native-memory value
read or external debug-file loading. Native-instruction stops and replaced or
stale publications must not reuse a preceding Wasm source position.

The new general-GC fixtures cover mutable numeric structs, two-node reference
cycles, numeric arrays and reference arrays, with allocation and mutation
families and independent scalar checksums. They do not qualify for immutable
compact admission. Root groups, actual table slots and reachable objects are
counted separately; planned allocation syntax is not an actual GC counter.
Official binary/WAT agreement and VM cold execution are pending. Pinned
Wasmtime copying/DRC and OpenJDK TLAB/G1 source studies inform a separate
precise-reference-metadata candidate; they do not establish our performance or
an industry ranking. It is excluded from the already frozen two-header r11
experiment. All new performance measurements must retain separate unprofiled
times, pure hardware counter groups and VTune hardware microarchitecture data.

The exact-O3 Windows split study has completed its six real commands. For the
four-function memory/musttail/SEH probe, direct COFF and unoptimized-bitcode
then O3-backend COFF are byte-identical, including checked code, unwind sections
and relocation targets. This is a small code-generation equivalence witness;
it does not qualify the ordinary full product build or Windows execution.
The ROS-only Windows VM input ISO and private overlays are prepared. Actual
paused launches have exposed QMP path-length and normal display-surface memfd
closure issues before guest execution; neither attempt is a product PASS.

Production embedded-DWARF indexing now declares the real DebugInfoDWARF static
component dependency in both build systems. The ordinary SDK shim advertises
it only when the certified actual archive exists. Earlier products and SDK
certificates do not qualify this new dependency; fresh configure/link and
source-authority/inline replay remain pending.

Precise reference metadata is implemented as a default-off source candidate,
with private per-type ownership and unchanged whole-graph/root/token checks.
Independent review also found and fixed the new exception graph test's bool
census callback contract. Its current native cold fixture explicitly covers
registered native wrappers, foreign exception recipient leases, aggregate/exn
cycles, full reference arrays, root drop, stale roots and invalid unreachable
immutable payloads. Its expected 5 successful/2 rejected collections, 8
aggregate reclamations and 1 exception retirement are test assertions, not
observed results. Both metadata variants still require actual matched-profile
native and sanitizer runs on Linux.


The 2026-10-02 precise-reference-metadata primitive cold replay has now
completed 12 actual Linux commands under the unchanged 64 GiB/swap0 cgroup,
with actual root exit status 0, all owned PIDFDs retired, an init-only final
roster and unchanged OOM counters. The ordinary candidate source fingerprint
was identical before and after. Raw-log and small-packet hash review confirms
37 metadata checks, 95 exception-graph checks for each metadata-off/on variant,
and 56 store checks for each variant. The two graph runs each actually report
5 successful and 2 rejected collections, 8 aggregate reclamations and 1
exception retirement; rejected collections reclaim nothing.

This is native component primitive execution, not VM root enumeration,
native throw/catch, a fresh ROS product, sanitizer or performance acceptance.
The metadata object requests 32 bytes and the mixed-struct reference-index
array requests 24 bytes; these are not process RSS measurements. The actual
compiler dependency list exposed a missing pre-build binding for the target
libc++ `__config_site`, which is recorded as a qualification limitation.
Static ELF dependency hashes are not claims about observed loaded mappings.
The current DWARF value and numeric setter changes require separate fresh
builds and do not inherit this candidate's evidence.

The primitive cold dependency follow-up also confirms every actual normalized
source/third-party include against the before/after fingerprint, including 889
files under the retained bundled third-party tree. Test files match the
pre-execution reviewed manifest expectations and actual dependency hashes, but
there is no separately persisted test-specific pre-build hash receipt. That
chronology boundary is retained alongside the missing target-config binding;
neither is replaced with a post-execution claim of complete build qualification.


The actual Windows ROS r3 attempt reached the Windows desktop and ran the
fixed bootstrap. Before any product case, the uploader omitted `-Repository
ros`; the unchanged schema-2 runner rejected the ordinary/ROS qualification
mismatch and reported an empty case list. This is a harness failure, not a
product execution result. The owned controller was cancelled, its QEMU/server
PIDFDs retired, the final cgroup roster contained only its init process, and
immutable inputs remained unchanged. OOM counters did not increase; memory
limit events did increase, so this is not evidence of absence of pressure.
The r4 derived ISO forwards only the missing repository argument and preserves
the qualified runner, PE, Wasm inputs and previous failed records. Its actual
guest execution is pending.

A separate source-map patch now retains the DWARF row discriminator, resets it
after each row and resets the full state at end-sequence. New DWARF4/5 fixture
rows cover column persistence, a full-width discriminator and sequence gaps.
Independent source review found no confirmed defect; native testing is pending.
The new finite source-step policy uses concrete inline DIE paths and statement
identity, with unknown physical activation rejected. It is a metadata component
only and is not integrated into current product next/finish behavior.

The default-off numeric struct.set32 candidate uses five register-width native
arguments and preserves actual canonical store, mutability, token, lease and
error checks. Its v4 IR test checks the actual RISC-V64 immediate-address call
path as well as external-symbol paths on other targets; production source is
unchanged from v3. Both source snapshots are retained. Native helper, LLVM
code-generation, official new-syntax and product/performance qualification
remain separate pending work. The frozen Stage2 numeric-source-value snapshot
also remains source-only until a fresh matching runtime/product build runs.

After the Linux reboot, performance admission must recheck actual cpu_core
hardware groups and VTune hardware collection under the existing cgroup.
Unprofiled user/system/wall times and actual frequency observations, pure
hardware groups, and VTune microarchitecture measurements are separate result
families. Profiler overhead or multiplexing must not be presented as the same
time measurement. No new administrator action has yet been required.

2026-10-02 current continuation: four fresh ordinary Clang23 O3/Werror header-only
source-debugger units passed on SSH Linux inside the actual 64 GiB/no-swap cgroup:
DWARF4/5 source rows/discriminators, finite stepping policy, concrete scope path,
and inline display through the shared selector. All 14 build/bind/run/provenance
stages succeeded with actual MD dependencies prehashed (including target libc++
config), before/after closure equal, child PIDFDs retired, init-only roster and
unchanged OOM counters. Evidence: `build/wasm3-evidence/debug-source-policy-current-cold-20261002-r2`.
The previous r1 failure remains retained: the synthetic step fixture used equal
zero physical/inline DIE identities; it now supplies distinct identities and adds
a duplicate-key rejection, preserving the original lexical-range expectations.
These are component tests, not real source read/activation/next/finish or ROS-native
product qualification. Fresh runtime/CLI/source-value and real activation tests
remain pending. The old Stage2 source-values freeze is retained; the stop-ID/DAP
numeric-leaf candidate and common-scope query have separate immutable manifests.

The current Linux boot's pure raw cpu_core cycles/instructions completed four
actual GC/EH diagnostic runs with exact enabled==running. Hardware VTune Hotspots
and uarch-exploration also completed; uarch MUX reliability is 0.561 and its total
hybrid-core counts must not be attributed solely to the P0 guest. Future candidate
runs separately record unprofiled user/sys/wall and guest time, pure hardware
counts, and hardware VTune with `-cpu-mask 0`, actual frequency and target identity.
Intel's CPU-mask CLI reference states the default collection mask is ALL:
https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/cpu-mask.html.
These fresh-boot runs establish hardware admission against the earlier frozen r9
products, not acceptance or a speedup of current GC/setter/EH candidates. A later
external swap-limit change was restored using the task's existing Docker resource
controls; actual kernel swap.max is again zero, with no administrator command.

Windows r4 reached its real desktop but no product case ran: the forwarding
uploader supplied Repository to a PowerShell runner which had not declared it.
Both repository runners now declare/validate Repository, bind result provenance,
require schema 2 for ROS, and select actual ROS `-Raot` versus ordinary full-JIT
arguments. The r3/r4 failures remain evidence; fresh derived qualification/ISO and
actual cases are pending. Host KVM open/API_VERSION=12 succeeded under the same UID,
while the task Docker cgroup's KVM open returned EPERM; no VM was created by these
read-only probes and no Windows guest has been run outside the resource cgroup.

2026-10-02 later continuation: the task sandbox was checkpointed, stopped and
preserved, then recreated with only its existing resource/security configuration
plus the actual KVM device permission. Exactly one task sandbox is active, with
64 GiB memory, zero swap and CPUs 0,2,4,6,16-31. The new actual init is 76565;
old receipts keep their original init/cgroup. A confined UID1000 read-only probe
actually opened KVM, returned API version 12, closed its owned descriptor and
created no VM/vCPU. No administrator action or global security setting changed.
Evidence: `build/wasm3-evidence/current-scope-kvm-restored-20261002-r1`. Windows
product cases still require the newly derived KVM launch and guest qualification.

The frozen Stage2 DAP adapter/source-value suites actually passed 19+9 methods
for each product, 56 total, on SSH Linux in the then-current 64 GiB/no-swap scope.
All six provenance/test stages returned zero; raw log hashes, retired PIDFDs and
before/after Python/source closure were checked. This is synthetic formatter and
protocol qualification, not actual JIT/source activation or native inspection.
Evidence: `build/wasm3-evidence/debug-dap-stage2-current-cold-20261002-r1`.

Current source stepping now consumes a joint runtime query which authenticates
the exact pause, publication generation, true activation chain and Code-relative
PC under one domain guard. The controller uses genuine entry/tail/return identity
for into/over/out, cancels pending steps on peer loss, and rejects malformed native
policy enums. A new aggregate-and-typed-tail witness requests actual precise root
IR before publication and retains live references across recursive calls. These
new controller/runtime/compiler sources are frozen separately for fresh matching
native builds; they have not passed real activation or C/C++/Rust product tests.
The previous synthetic scope-policy pass does not qualify this new implementation.

The set32 v4 native component passed 981 checks with the six-switch profile off.
The profile-on compile exposed a missing outer macro scope in that test fixture;
the failure is retained and no profile-on run was claimed. The v5 fixture matches
the existing runtime macro include contract, with unchanged GC/setter production
bytes and all other v4 pins. Both new profiles and graph/store tests are queued.
The private native-EH leaf candidate now carries actual fused call provenance,
a separately owned staged IR clone and exact numeric no-trace ABI. Real native
publisher/CFI/extent/source-generation binding is under implementation, so this
source work does not yet establish runtime selection or a performance improvement.

2026-10-02 GC v5 continuation: all 23 fresh native build/bind/run and closure
stages passed inside the restored 64 GiB/no-swap Linux scope. Both setter
six-switch profiles passed 981 checks and reclaimed two objects. Trace metadata
profiles 0/1 each passed 95 graph/exception checks (five successful collections,
two rejected malformed graphs, eight aggregate objects and one exception retired)
and 56 reference-store checks (three collections, eight objects reclaimed).
All root return codes were zero, actual PIDFDs retired, OOM counters unchanged,
and source/tool/MD/output closures equal. Peak owned RSS was 2,043,588,608 bytes.
Evidence: `build/wasm3-evidence/gc-set32-v5-native-current-cold-20261002-r2`.
The previous compile and missing-before-image preparation failures remain
retained. This pass qualifies ordinary native GC primitives; ROS-native execution,
LLVM lowering, full VM roots, sanitizers and performance remain separate work.

The actual synthetic continuation-policy unit also passed its five fresh
compile/bind/run/closure stages, including typed tail successor and recursive
identity comparisons. Evidence:
`build/wasm3-evidence/source-step-continuation-current-cold-20261002-r1`.
It supplies no runtime permission or native stepping qualification. A new real
C/C++/Rust source-step runner is frozen separately; it checks official embedded
DWARF4/5 against actual product stops, into/over/out, nested inline instances,
repeated call sites and recursion. Its real product execution is pending a fresh
matching controller/compiler/runtime closure.
