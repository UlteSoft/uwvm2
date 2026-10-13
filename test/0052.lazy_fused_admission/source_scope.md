# Mandatory fused lazy admission — PRIVATE SOURCE R4-r2

No live source writes or native compilation/execution. Source/BMI/LLVM verifier,
code generation, runtime behavior, peak resources and performance are unqualified.
The root is the only live writer; Linux native tests require its existing keeper
and original 64 GiB cgroup. Mac compiler/runtime/QEMU execution is prohibited.

## Problem and implementation

Core 3 [module validation](https://webassembly.github.io/spec/core/valid/modules.html#valid-module)
requires every contained function to be valid, even if no execution ever calls it.
The existing plain INT/LLVM lazy initializers admit only declaration/index shape;
body typing occurs only at first materialization. Consequently unused malformed
function bodies are not reliably rejected at module admission. The text facade
`lazy+verification` additionally runs a pure validator before translation.

This candidate moves the existing backend fused validator/translator forward.
It never uses a pure validator to manufacture an emission permit. INT retains the
actual register-ring dispatch stream, fixups and original checked call metadata;
its later scheduler callback only checks the original immutable descriptor,
reset generation and exact template layout before ready publication. This means
INT translation latency and dispatch allocations move to startup; it does NOT
claim startup speed or memory parity. Its former finer EU/CU scheduling policy
is explicitly normalized to whole-function units, because the actual checked
artifact remains a whole function. No guest opfunc/musttail/memory access changes.

LLVM emits each body during admission into a temporary actual module/context,
forces verifier success and serializes exact bounded compiler-owned bitcode. The
context is destroyed only after its module. Source, function/signature descriptor,
reset generation and emitted call ABI/target table addresses are bound into the
private plan. All later lazy grouping uses typed dependency DATA copied AFTER the
original same-op signature/operand checks, never a source byte scanner.
The compact lazy object-cache key now uses the retained bitcode SHA, avoiding
an additional source-body byte hash at first materialization; the existing
ObjectCache still authenticates the complete emitted-module context. Actual
IR consumption parses LLVM bitcode (not Wasm), links selected fragments, verifies
again and retains the original MCJIT optimization/codegen/publication path.

The runtime must first build the index and then allocate the real final target
slots; only then can it execute fused admission. No background context, worker or
guest entry is published before the latter succeeds. Every unused body is checked
before returning initialization success. Both lazy CLI spellings now use this
same mandatory admission; standalone `-m validation` remains unchanged.
The CLI already defers active segments. It now uses a lazy prepare host entry
before applying ANY preload/main active data/element segment or invoking a
start function, mirroring the existing full prepare contract. The actual lazy
initializer owns admission/FP/publication scopes; its later ready check prevents
a second body walk. This does not promise atomic file versions or roll back
existing declaration/import allocation and immutable-global preparation.

ROS intentionally has only full backends. Its only production delta is the paired
optional DATA type/export and existing full LLVM fused call sink; default null
arguments create no dependency entries, allocation, metadata members or guest IR. Ordinary lazy machinery is not
introduced into ROS. Old wasm1p1/wasm2 pure validators are unchanged in both repos.

## R1 to R2 correction

Dependency sinks are optional explicit arguments of the original fused function
invocation. The lazy factory owns their bounded scratch, rather than adding a
vector/flag to every full function metadata record. Original full function
metadata and compile-option layouts therefore remain unchanged. There is no
additional default dependency allocation or instruction; fresh source/module
interfaces still require compilation because the inline API has new tail arguments.

## Resource and authority boundaries

INT bounds retained dispatch bytes to 8 MiB/function and 128 MiB/module, plus a
65,536-function source-index cap. LLVM bounds each bitcode output to 8 MiB and all
retained record/bitcode/dependency DATA to 128 MiB; typed dependency scratch is
bounded to 65,536 events/body. Dependencies are copied into exact owned arrays;
the bounded transient vector is destroyed with that same call iteration. One additional bounded bitcode scratch
allocation is necessary while writing. These are compiler-payload limits, not a
claim about malloc overhead, emitter/validator scratch, EH metadata or total RSS.
All later bodies still run the authoritative checks after quota exhaustion; no
partial plan succeeds or silently reparses source. Resource failure refuses
admission separately from malformed Wasm. Legal native lowering decline remains
an unavailable per-function native artifact and never masquerades as invalid Wasm. Existing FastIO no-throw allocation
failure policy is retained; C++ owned allocations unwind by RAII where enabled.

The cold reset-generation equality alone is NOT source ownership, a generation
lease, stop permission or protection from racing reset. Existing admission/
publication serialization and worker drain pin runtime descriptors and immutable
source across compilation. Plans cannot be synthesized from arbitrary records:
constructors are private and factories invoke the actual fused translator.

## Explicit incomplete scope (DO NOT APPLY AS WHOLE TASK COMPLETION)

* INT's original full translator still has raw trivial-call matchers, optimizer
  lookahead and loop-unwind replay. This candidate removes the deferred second
  body translation and structural scanner, but it does NOT prove every source
  opcode/immediate was decoded exactly once inside that existing translator.
  Removing these rereads requires checked-event optimization without disabling
  fast paths or changing ring performance.
* The full LLVM dispatcher still contains a legacy raw-instruction fallback
  when an opcode handler does not mark inline emission. Most normalized families
  seal it, but every supported Core3 path has not been exhaustively proved in
  this candidate. It is not silently disabled to reject legal instructions.
* Tiered T0/T1 initialization shares the new actual artifacts and exact T0 layout;
  existing T2 still calls the full raw-body walker. Simply substituting lazy-table
  call IR would change T2's direct-call lowering/performance, so it is deliberately
  left for an explicit checked-IR specialization/whole-engine owner implementation.
* Both T0 interpreter fallback and T1 LLVM admission currently traverse separately
  to produce their distinct backend artifacts. This does NOT fulfill one shared
  typed decode feeding both backends. The PRIVATE retained-event EH/control/ref
  chain supplied by root is not yet a complete all-Core3 emission interface.
* Native availability is separate from complete semantic admission. R4 seals all
  typed bodies even when the original LLVM emitter cannot lower a legal unused
  function. Such a function and its primary CU become `failed` before workers or
  guests can observe admission; no ready native entry is published. Existing
  plain LLVM demand still reports its real native-unavailable result, while only
  the existing tiered-T0 path may execute its separately sealed exact-NTTP INT
  dispatch artifact. This does NOT implement plain LLVM fallback, nor call an
  unsealed interpreter option, nor promise all-Core3 native lowering support.
* Existing lazy tests that expect deferred invalid-body checks/multiple split units
  need revised expectations, and parameterized INT test initializers must pass
  the same ring template option later used by the execution request. The one
  existing LLVM callee-query call is adapted to its actual admitted storage.
* These cold changes can affect startup latency, memory peaks, scheduler behavior
  and LLVM object-cache hit costs. No performance guarantee is inferred from source.

## R3 to R4 correction: validity versus native availability

The private factory retains the original fused emitter's first native-decline
DATA separately from the validation diagnostic. `admission_available` requires
that every body was typed and all retained-record/dependency/bitcode quotas held.
`native_ir_available(index)` additionally requires that exact function's actual
nonempty bounded verified bitcode AND no original native decline. A preparation
decline can leave a declaration-only module whose verifier succeeds; it is never
retained as an available native body. There is no `all_ir_complete` module gate.
A bad later body is still checked even after an earlier legal native decline;
allocation/quota failure never becomes successful admission.

Before claiming any group member, the materializer excludes native-unavailable
records. A mutable-table conservative unwind cohort can therefore contain a
legal uncalled declined function without failing an available demanded entry.
Direct-call dependency retention now happens after the actual fused tuple check,
even if native lowering was disabled earlier in the same body; default full/ROS
calls still pass a null sink. Complete source/generation/ABI equality remains
mandatory. Native-unavailable selections refuse before allocating a merged LLVM
owner; no raw Wasm replay, additional matcher or second body walker is introduced.

`native_decline_available_entry.cc` and its two WAT files provide a real emitter
case: validation exceptions are enabled while the supplied native-EH TargetMachine
is intentionally null. A reachable uncalled `throw` must genuinely decline native
emission; an available entry with a constant-false direct-call arm must consume
retained bitcode through production grouped MCJIT and return 42 for instruction
and unwind policies. It also requires the unavailable selection to leave merged
IR unallocated, and a subsequent unused `i64; ref.i31` body to produce a genuine
validation pointer inside that later body. This is an ordinary lazy component;
paired WATs also support full-backend comparisons, not nonexistent ROS lazy modes.
No part has compiled/run. The unavailable-demand component observes the actual
compiler state; it does NOT qualify the plain CLI fatal diagnostic or real T0
fallback execution, which require their separate existing-runtime fixtures.

## Meaningful planned qualification

`test/0052.lazy_fused_admission/run_unused_body_admission.py` has 12 independent
WAT cases: seven unused invalid Core3 bodies (i31, memory64 address, nondefaultable
local, call_ref operand, try_table catch label, GC struct field, and invalid-body-before-active-OOB ordering), plus five valid
modern programs (real typed recursive tail calls, memory64+GC+throw/catch, typed
non-null select, unused nested dead code and memory64/table64 active segments). wasm-tools assembles and independently
classifies exact binaries. Ordinary full/lazy/lazy+verification and tiered lazy
are tested; ROS has only its actual full pair. Invalid `_start` never calls the
malformed body. Only a genuine validation diagnostic qualifies; parser/CLI failure,
resource decline, crash or timeout cannot be counted as conformance rejection.
Positive execution checks its own arithmetic/memory/exception results. The unused
nested-dead positive checks admission/emission, NOT actual dead-code execution.

`admission_negative.cc` uses the actual parser/initializer and no pure prepass.
All six failures must come from a diagnostic pointer inside the actual uncalled
penultimate body, never the empty final start or declarations. Header-only and
module consumer compilation require fresh paired full interfaces/runtime/main and
new LLVM/context/helper closures; old binaries cannot qualify the metadata layout.

First keeper recipe after root source review: fresh all3/BMI, independently assemble
all 12 WATs, run the bounded matrix above and negative component; then dedicated
IR/module ownership/bitcode quota failure tests, codegen/optable ASM equivalence,
startup RSS/latency and steady-state P0/HW performance. None has run in this packet.


# R4-r3 checked unwind import routes and oracle correction

SOURCE ONLY: no native compile/run, BMI, LLVM verifier execution or timing result.
The immutable R4-r2 remains at manifest902861fc; this successor changes neither
mandatory typed admission nor legal per-function native-decline policy.

## Actual call-path cost findings

Ordinary lazy admission has original route=false and both real lazy target tables.
The actual call emitter's has_lazy_defined_target_tables branch still emits a
volatile typed-slot load, nonzero guard, typed Wasm call when ready and raw target
materialization only when zero. Changing the old group's route flag did not
replace all local unwind calls with generic VM calls. Tiered uses route=true.
The previous fully claimed unwind cohort temporarily forced route=false; its
same-module forwarded imports could therefore use typed targets, while unmodified
R4-r2 would always use the generic imported-call bridge for that case.

R4-r3 fixes that precise route through real retained SSA: only the original fused
factory's nonnull dependency sink enables staging, and only routed+unwind imported
aliases resolving to an actual same-module local declaration with equivalent rich
Core3 type/LLVM ABI get the two CFG alternatives. The condition starts constant
false, retaining the original generic import path. Ordinary full and ROS have no
staging sink and emit no alternative IR; compile_option and local_func_storage_t
ABI remain unchanged. ROS has no lazy/tiered target tables, so this emit-only
helper and compiler state bit exist only in the ordinary backend.

After the original complete candidate cohort obtains compiling claims, the exact
plan/scheduler/source/runtime-generation owner consumes the tagged compiler CFG.
It checks actual current import alias/local descriptor, rich type equivalence,
LLVM CC/complete parameters/results, owned function constants and actual target
slot extents; target must be in this genuinely claimed native-available cohort.
Only then can its compiler constant become true. Partial/declined/missing target
cohorts preserve the original fallback. All checks precede IR condition mutations,
then forced final verification precedes any engine/native publication. No Wasm
body, LEB immediate or opcode matcher is replayed. This does not grant callable,
native PC, debugger, CFI, mutable table or restore authority.

The existing non-atomic already-ready constant-pointer bake is a separate known
SOURCE cost gap: admission occurs before any target publication, so all retained
slots initially produce the existing load/branch/indirect path. Earlier deferred
emitters could bake an already-published non-atomic typed pointer. Tiered atomic
slots never allowed that bake. Checked-IR specialization is still needed for that
ordinary-lazy case; no measured regression amount or all-T2 closure is claimed.

## Meaningful added planned qualification

unwind_import_route.cc parses two actual modules and initializes a real foreign
provider. Its same-module alias is explicitly rebound only as cold fixture setup,
not guest mutation or initializer-alias qualification. The original fused walker
must produce real tagged SSA from this actual declaration and verify its owned
bitcode. The test requires unclaimed preflight refusal with original false branch,
real compiling-cohort selection/LLVM verification, missing-target fallback,
production grouped MCJIT/actual typed-slot publication and real i64 result42;
the foreign provider would produce99. The source includes memory64 syntax.
These tests have not compiled or executed.

The paired finite runner now requires wasm-tools negative validation to carry the
actual BinaryReaderError `(at offset 0x...)`; explicit features=all, unknown CLI or
feature, I/O failure, panic, timeout and negative signal status cannot qualify.
Product validation refusal requires positive exit status plus its actual code
validation diagnostic. This does not count arbitrary subprocess failure as PASS.

Primary references: https://webassembly.github.io/spec/core/valid/modules.html#valid-module
and https://llvm.org/docs/LangRef.html#metadata . Pinned LLVM23 BasicBlock/Metadata
and runtime descriptor/type helper source is included as read-only dependencies.

## R4-r4 moved-owner correction

The R4-r3 raw factory-stack scheduler-address check was a SOURCE blocker: a
legitimate move or non-elided named return invalidated that address. R4-r4 removes
it. Exact private unique_ptr allocation identity is stable, while actual complete
function/materialized vector counts, local/public indexes, primary CU/source
descriptors and both function/CU compiling states are checked before selecting
any IR path. The actual source/generation/ABI plan checks remain mandatory.
The unit explicitly moves the initialized storage, proves transfer of the same
private plan and exercises both unclaimed refusal and real grouped publication.
No native/source qualification is inferred from that source-only fixture.
