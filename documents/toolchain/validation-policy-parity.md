# Validation policy and backend parity audit (2026-09-16)

This audit compares **the same version and feature policy**, not just the default
settings of different APIs. The binary module parser is shared; the interpreter
and LLVM translators retain their own operand/control stacks for code generation.
Tests therefore call those internal validators directly as well as the standard
prepass and real CLI paths. A successful prepass alone cannot establish parity.

## Rules that must not regress

The references are the frozen [Core 1 specification](https://webassembly.github.io/spec/versions/core/WebAssembly-1.0.pdf)
and [Core 2 specification](https://webassembly.github.io/spec/versions/core/WebAssembly-2.0.pdf),
especially appendix 7.3, sections 3.3/5.4.1, and Core 2 section 7.2.2.

- `end` restores the enclosing frame's validation-unreachable state. It does not
  merge the execution reachability of the two `if` arms or propagate a loop's
  terminating body. Both `if` arms can use `br 0` and reach the continuation;
  neither supplies an otherwise missing operand after the `end`.
- A polymorphic `select` still pushes **one** value. Unknown is a type, not absence
  of a stack slot. It must participate in arity checks, while matching any expected
  type. Conversely, a known select operand constrains an Unknown operand.
- `local.tee` and `br_if` produce their declared types even when consuming Unknown.
  Leaving the result Unknown incorrectly admits a later incompatible consumer.
  Interpreter refinement uses its pop/push helpers so the placeholder's byte
  width does not corrupt operand-stack accounting.
- Unreachable permits missing deeper operands, not a wrong concrete suffix of a
  multi-result tuple. `else` and `end` must check that suffix too.
- Untyped `select` accepts numeric/vector types, not concrete references. Typed
  select and reference consumers must still accept genuine Unknown operands.
- Core 1 requires identical `br_table` label types. Core 2 additionally permits
  labels of equal arity whose arguments can meet through bottom. The explicit MVP
  policy selects Core 1; disabling proposals under a newer policy does not.

Validation state and actual interpreter code-generation reachability remain
separate. These repairs add no guest-loop checks and no new whole-module prepass.
They do not remove ROS's full-only execution restriction.

## Reproducible tests

- `test/0012.validator/validation_policy_corpus.cc`: batch parser/validator probe;
  arguments are API (`wasm1`, `wasm1p1`, `wasm2`, `runtime`) and feature profile
  (`mvp`, `wasm1p1`, `wasm2`), with binary paths on stdin. It never instantiates or
  executes guest code, so linking or a trapping start cannot obscure validation.
- `test/0012.validator/check_validation_policy_corpus.py`: checks explicit APIs
  against official WAST binary assertions, with conversion failures fatal and
  text-only assertions recorded separately.
- `test/0014.llvm_jit/validation_control_backend.cc`: batch direct standard,
  interpreter-compiler and LLVM-internal validation of the self-contained fixtures.
  This bypasses the normal compiler prepass on purpose. ROS uses its reduced
  LLVM validator signature, without restoring lazy/tiered parameters.
- `test/0012.validator/check_validation_control_parity.py --bottom-matrix`: fixed
  control-flow/type-refinement cases, malformed binary immediates, and 490
  deterministic Unknown/concrete/select/consumer combinations. Invalid functions
  are actually invoked, including defects after an initial `unreachable`, so a
  premature runtime trap cannot be mistaken for correct validation.

Pinned upstream test sources:

| Standard | WebAssembly/spec commit | Binary cases | Text-only exclusions |
| --- | --- | ---: | ---: |
| Core 1 | `fd4fe9f5f271740d22f0fe82aff383086fef7b11` (`wg_v1`) | 2,527 | 430 |
| Core 2 | `fffc6e12fa454e475455a7b58d3b5dc343980c10` (`wg-2.0`) | 4,581 | 1,091 |

Core 1 conversion uses WABT 1.0.8, with 1.0.13 as a conversion-only fallback for
`binary.wast` and `const.wast`; the upstream assertions remain the oracle. Each
case records which converter was used. Core 2 uses WABT 1.0.36.

Do not substitute WABT 1.0.36 with all `--disable-*` options for the Core 1 oracle:
it accepts some newer SIMD instructions/segment forms and retains newer bottom
typing. It also accepts padded negative block types such as `c0 7f`, although the
Core 2 binary grammar permits literal `40`, a one-byte value type, or a
non-negative s33 type index. That disagreement is explicitly logged by the test;
it does not change the normative invalid expectation.

The paired official-corpus run passed 60,972 API/module checks: three MVP APIs
per product and five newer-profile/API pairs per product. This is binary
validation coverage, not 60,972 independent modules or a complete specification
certification. Actual execution is checked separately from acceptance/rejection.
The final v6 probes repeated all 60,972 checks successfully, and the direct
backend/CLI control matrix passed **14,438 checks with zero failures**. This
includes the 490 generated bottom/refinement combinations and malformed binary
immediates; it is not just an agreement test between two potentially wrong VMs.

## Actual execution and cache replay

The final `latest.parity6-NUoKhe` release/O3 header builds of both products passed:

- Core 2 execution corpus: 1,148 wrapper cases on each of six paths (main
  int/full, LLVM/full, int/lazy, LLVM/lazy; ROS int/full and LLVM/full), for
  **6,888 CLI runs and 249,456 assertion executions, zero failures**. These are
  repeated assertions across modes, not that many independent specification cases.
  The harness explicitly records 4,673 exclusion records for foreign module graphs,
  host-created non-null references and later stateful actions after fatal traps;
  grouped exclusion records can account for multiple actions. Text-only WAST
  assertions belong to the separate exclusions above.
- Recursive trap/cache matrix: **216 CLI runs**, including **144 authenticated
  full-JIT replay runs** in fresh processes. Each product covers six LLVM policies,
  instruction and native-libunwind tracking, and unreachable/OOB/float-to-int
  traps. Tests require the exact eight repeated recursive frames plus the entry
  frame, verified signatures and a live native-unwind provider when requested.
- The execution audit exposed an independent LLVM 23 X86 mixed-signedness
  conversion regression. The paired fix passed **31 additional CLI runs / 248
  bit-pattern assertions**, including **12 authenticated replays**, all six full
  policies, all four lazy policies, and interpreter controls. See the
  [conversion investigation](x86-llvm23-mixed-conversion.md) for its independent
  IR reproducer and **3,752** SSE2/AVX2/AVX-512/x87/i686 generated-code checks.

The execution harness accepts `--full-mode full|lazy --products full|ros` so a
lazy run need not silently repeat ROS's full-only coverage. This does not add
lazy execution support to ROS.

## Generated code and bounded performance checks

The main raw-body lazy scanner unit, both products' Wasm 2 feature/backend parity
units, and six standard typed-select/table-policy unit runs passed. Interpreter
hot-op probes for reference, conversion, SIMD, table and bulk-memory operations
produced byte-identical assembly before/after and across both products (SHA-256
`8ad7d929b101cbc7779b56b285182df95b60e3018dd69b46d229a3b657ba3fe0`).
These interpreter sources did not change between the v4 and final v6 snapshots.

For the scalar rotate/xor, f64-add and i32x4-add/xor benchmark, both products'
complete before/after JIT object files are identical within each paired run.
The 1,574-byte generated code section has SHA-256
`8eeb462e869cfbe5b6911fe2de99ccd5a2297431313be1168c7a21c5b7de4897`.
The actual kernel symbols (not raw ABI wrappers) have sizes 228, 105 and 173
bytes respectively. Inspection confirms native register-only loop bodies with
no loop-internal calls or stack traffic; constant loads precede the loops.

Timing uses WASI monotonic timestamps inside the guest, excluding compilation
and process startup, with exact result checks, two warmups, alternating old/new
binaries and CPU 16 affinity. No compilation job ran during these measurements.
Ratios below are **after / before median elapsed time**, so lower is faster.

| Product/backend | 1M iterations, 7 pairs | 10M iterations, 15 pairs |
| --- | ---: | ---: |
| Main uwvm-int | 0.9780 | 1.0227 |
| ROS uwvm-int | 1.0317 | 0.9872 |
| Main LLVM | 1.0001 | 0.9998 |
| ROS LLVM | 1.0001 | 1.0003 |

The interpreter differences change direction between runs; these observations
do not establish a consistent speedup or zero regression. LLVM is unchanged in
both measured code and these timings. This small kernel set is not a guarantee
about all workloads, conversions, instruction combinations or ISA variants.

## Lazy validation and cache boundaries

Core 2 permits a function's validation to be deferred until its first call. The
entire body must then validate before **any** instruction in it executes. Thus an
uninvoked invalid function can be rejected earlier by full mode than by lazy mode;
that timing distinction is not permission to accept different function types.

Public full compilation crosses `validate_code_with_runtime_policy`; lazy paths
use that same policy before materializing a function. Options that assert prior
validation remain internal caller contracts, not validation bypasses for untrusted
input. LLVM object-cache lookup occurs after validated IR construction; cached
native objects must not make malformed Wasm acceptable. Source/ABI/IR fingerprints
and authenticated loading retain their existing protections.

## Scope and evidence

Linux builds and tests run under one aggregate cgroup with `MemoryMax=32G`,
`MemoryHigh=28G`, `MemorySwapMax=0`, and CPUs 16–31. Heavy C++ builds are serial.
No limit is borrowed from the other agent's separate allocation.
The shared campaign cgroup's recorded peak was 31,057,932,288 bytes (28.93 GiB),
below the enforced 32 GiB maximum.

Final production snapshot: `latest.parity6-NUoKhe`, manifest file hashes
`4fb3fc752e4b49b13dc1e856d0db556f819ca9375076df145ac1bb129500e565`
(full) and `1d3ae56d221c9a1032309bce80b509aed2911fd8aedf1411e1d59e3e07aa22ed`
(ROS). A subsequent SHA-256 comparison of all 1,663 main and 1,610 ROS production
files found no workspace drift. Later test/report-only additions do not alter
these CLI production sources. Evidence directories use the `validation-v6-`
prefix; earlier `validation-*-v4` runs predate the X86 conversion workaround.
Remote evidence is under `/tmp/uwvm-comprehensive.eUFnxH/`, with local frozen
sources under `/tmp/uwvm-comprehensive-audit.i2TDbc/`.

The snapshot includes earlier uncommitted work, particularly ROS's complete
LLVM Wasm 2 lowering. Passing a frozen workspace is not proof that cherry-picking
only the small validation diff onto an older ROS commit supplies those dependencies.

This run does not establish every ISA, OS, build switch or physical machine.
QEMU/backend tests in the earlier MIPS investigation are separate evidence, not
proof that this native parser campaign executes every architecture.

The separately submitted MIPS backend repair is
[llvm/llvm-project#223905](https://github.com/llvm/llvm-project/pull/223905).
Submission is not upstream acceptance; see [the backend repair notes](mips-r6-llvm-backend.md).
