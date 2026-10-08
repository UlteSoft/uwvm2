This candidate specializes static local calls in the already owned T2 IR. It
removes T2's target-slot reads for that subset; it does not claim the old T2
wrapper/frame layout, a measured performance gain, or one tiered typing walk.
All source and native qualifications remain separate.

Actual current traversal boundary

* INT lazy admission calls checked_register_ring_admission::admit<CompileOption>
  and retains the original full register-ring output, checked call metadata and
  its fixups. Demand-time materialization consumes that artifact.
* LLVM lazy admission calls the original full validator/emitter for every local
  function and retains its exact owned bitcode and same-walk metadata. Demand
  compilation consumes LLVM bitcode rather than source slices.
* The T2 callback consumes ALL of those sealed LLVM fragments. It no longer
  calls compile_all_from_uwvm over the Wasm bytes a third time.
* Tiered T0 INT admission and LLVM admission still each enter their backend
  walker. Shared typed stack kernels and normalized instruction families do not
  turn those two traversals into one. Existing interpreter lookahead/loop replay
  also remains a separate source-decode limitation. No benchmark may label the
  current tiered pipeline "one validator walk".

Implementable genuine dual-sink boundary

A common typed frontend must run the original checked opcode transaction once,
perform its bounded immediate decode, type/control transition, feature checks,
local-initialization checks and exception-label checks, then emit an owned typed
operation directly to both actual backend sinks in that same transaction. It
must not call the pure validator first, trust arbitrary caller DATA, or retain a
raw slice for a later sink to decode. The event sequence is private to that
frontend/source/feature owner, and is sealed only after all body end/control
and declaration requirements succeed.

The sink contract needs more than numeric values: original source offset,
validated index/type signature, frame identity/height/polymorphism, current
reachable state, exact input/output abstract types, definite-assignment state,
validated EH catch/branch targets and call effect, and immutable memory/table
index/address-width metadata. A source byte pointer is diagnostic-only; neither
sink may dereference it to recover an immediate. The frontend is the sole
issuer. Failed transactions cannot publish either sink's partial artifact.

The INT sink must be extracted from its current ring emitter without copying
another validator: consume the actual typed inputs/results and control events,
maintain physical register-ring carriers/spills/patch offsets, preserve musttail
and canonical call fixups, and resolve branches from event frame identities.
It must replace current raw lookahead/loop replay with typed-event lookahead or
owned typed loop plans, with bounds before each event cursor/index change.
The LLVM sink lowers those same typed events into existing SSA/PHI/Invoke/cleanup
helpers. It must not call inline stack matchers or reopen source immediates.
Its SSA/block ownership is private, distinct from INT's physical carrier state.

The smallest truthful vertical slice is a function wholly contained in already
normalized numeric/constants/local/select events, with a final end transaction.
Mixed functions are not claimed single-walk until all control, reference, GC,
memory/table, bulk, atomics, calls/tail calls and modern EH events have actual
consumers. An unsupported legal event must be a typed backend availability
outcome, never invalid Wasm or a covert raw replay. The larger migration must
preserve the same shared abstract subtype/Bot semantics and diagnostics across
pure validation, both INT modes and both LLVM modes. Every modern negative,
including an unused last body, must be rejected before initialization effects.

Allocation and normal-path contract

The private event owner needs a checked function/event/byte quota, complete
RAII rollback of both unpublished sinks, and no live runtime code/publication
permission in its records. Successful native publication still belongs to the
actual execution-domain/source/runtime generation and engine owner. Allocation
exhaustion cannot certify successful admission. Default full/null sink should
not retain an event tape or add guest hot-path polls/locks/guards. Validation
work moves to admission, while native codegen/publication remains genuinely lazy.

Actual qualification required

Meaningful keeper tests must count actual original body decode/semantic entry
instrumentation in an untimed developer build, confirm one per function for
both sinks, inspect INT native dispatch/musttail and LLVM SSA/Invoke/optimized
code, and run modern legal/illegal unused bodies plus later lazy calls, recursion,
typed references, tail calls and exceptions. A generic event DATA unit or absence
of a second T2 call site is not that qualification. Performance requires both
startup cold latency/RSS and P-core wall/hardware-counter measurements; this
source candidate provides no numeric performance claim.

Primary semantics:
https://webassembly.github.io/spec/core/valid/instructions.html
https://webassembly.github.io/spec/core/exec/instructions.html#function-instructions
https://llvm.org/docs/LangRef.html#metadata
https://llvm.org/docs/ProgrammersManual.html#replacing-an-instruction-with-another-value
