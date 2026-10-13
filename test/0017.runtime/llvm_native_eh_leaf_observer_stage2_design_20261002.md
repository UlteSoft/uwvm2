# Stage 2: observations from the actual fused LLVM traversal

Status: read-only source design, 2026-10-02. Stage 1's
`wasm_exception_private_leaf_effect.h` is not connected to production. This
document implements nothing and reports no native result or speedup. The
frozen struct.set32, r11 collector and precise-trace candidates are separate.

The next patch should collect genuine completed instruction effects and
ordinary direct-call witnesses during the existing validation/translation
pass. It must leave every emitted call, throw, trace, cleanup, frame, native
symbol and publication route unchanged. Private copies and trace omission
remain a later stage with their own proof and loaded-code ownership.

The Core 3 rules validate every try_table clause and match the ordered
handler against the thrown instance's tag; reference catches also expose
the exception reference. The observer must therefore use actual retained
tag instances and the first matching clause. A type/signature/index is not
that identity. See the official [try_table validation](https://webassembly.github.io/spec/core/valid/instructions.html#valid-try-table)
and [throw_ref execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-throw-ref).
The official pages read for this design identify WebAssembly 3.0,
2026-10-01; this document does not replace those validation rules.

## Initial admission and output

Use a new exact-1, default-off gate, proposed
`UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER`. It is neither the pending
numeric ABI switch nor a trace-specialization switch. Under that gate only,
an explicit `compile_option.record_native_eh_leaf_observations` request
defaults to false. Require actual `compilation_mode == full`, a qualified
native exception TargetMachine and prepared `state.native_guest_exceptions`,
no pending plan, no routed/runtime/raw/lazy target tables, no OSR entries,
and no debug observer/safe points/patchable target slots or armed management
admission. A missing fact declines recording and preserves normal compilation.
Do not infer full mode from this emitter also being used by T2.

Keep the initial module closed: no imported functions, tags, memories,
tables or globals and no host callback route. Use the initialized source
owner already retained by the full compiler caller to keep the runtime
module and local records alive throughout the observation. Borrowing a
module pointer or creating an observation_domain is not source authority.
Stage 2 never creates a full_source_instance, a generation permit or an
executable optimization certificate.

Proposed gated result fields:

- `local_func_storage_t.native_eh_leaf_observation`: optional sealed Stage 1
  function observation; owned tag lifetimes and copied handler clauses.
- A local vector of `direct_call_witness { expression_offset, event_ordinal,
  observation_callsite_index, actual_public_target_index,
  fragment_ordinary_call_observed }`. These are bounded integers and
  recording facts, not persistent LLVM pointers or machine addresses.
- A per-module `native_eh_observation_result` with one fresh traversal
  domain, function slots, attempted/completed/declined counts and a decline
  reason. `complete` means every slot came from this actual successful
  fused traversal and the final module succeeded; it does not admit code.

Use the existing public `function_index` consistently. In this first
no-import scope it equals the local index; still check that equality from
actual import count and local vector membership. Never mix local indices
with a domain that counts public functions if imports are later admitted.

## Exact hook positions

All paths below are relative to `src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/`.
The offsets are orientation for the currently reviewed source; function
names and control-flow locations are authoritative.

| Actual location | Minimal proposed hook |
| --- | --- |
| `single_func.h::validate_runtime_local_func`, existing signature/local construction around 648-707 | Classify the already available rich parameters/results and decoded local records. Require i32/i64/f32/f64 for leaf eligibility; ref/v128/unknown rich types decline the leaf. Do not infer rich numeric types from flat reference carriers or reparse local declarations. |
| Same function, after `try_prepare_runtime_local_func_llvm_jit_emit_state` and current Core 3 declaration-policy checks | Begin a per-function recorder with actual expression length, checked function index, this attempt's owned domain and prepared native scope. No byte scan. |
| `single_func_validation_dispatch.h`, after the switch and optional `try_emit_runtime_local_func_llvm_jit_instruction` | Commit exactly one prepared validated event with `[instruction_begin-code_begin, code_curr-code_begin)`. Only successful validation plus still-active native emission can commit. |
| Same dispatcher, `disable_inline_llvm_jit_emission` | Clear/drop the observation and all pending event/witness state in addition to its existing original module reset. Never seal a prefix after validation continues on the fallback path. Observer decline itself must not call this function or discard otherwise valid IR. |
| `opcode/control_flow_cases.h`, try_table after `read_handlers` and typed `enter_control_frame` succeed | Prepare the event from `control_flow_stack.back().exception_handlers`, using its authenticated resolved tags and numeric block signature. Commit it at the common post-switch hook, not twice. |
| `opcode/branch_cases.h`, throw after `resolve_tag` and full rich payload operand checks, before branch repair/truncation | Prepare the actual tag/payload class plus a snapshot of all active validated handlers in inner-frame-first, original-clause order. Preserve the original throw-vs-local-branch decision. |
| `opcode/call_cases.h`, ordinary call after exact target/argument/result validation | Prepare its checked public function index and active handler snapshot. Keep its opcode origin as ordinary `call` distinct from return_call/call_ref/indirect routes. |
| `single_func_emit.h::emit_runtime_local_func_llvm_jit_direct_wasm_call_value`, final ordinary typed declaration callback | After `emit_runtime_local_func_llvm_jit_may_throw_call` returns a real CallBase, record a same-operation witness only for the matching prepared ordinary-call event and actual same module/target. Debug and pending branches are excluded. |
| `opcode/control_flow_cases.h`, outer function end immediately before its internal return | After trailing-byte/result/local checks and successful `try_emit...end` plus `finalize_runtime_local_func_llvm_jit_emit_state`, commit the final end range and seal. The common dispatcher tail is never reached here. |
| `single_func.h::compile_all_from_uwvm_local_func`, only after the fused call returns successfully | Transfer the sealed local record into its checked output slot and mark actual function completion. Error/exception paths do not set completion. |
| `compile_all_from_uwvm`, only after all worker tasks, link and final module verification succeed | Merge/close every slot from the same attempt domain. Empty, duplicate, wrong-domain, missing, non-emitted or failed slots cannot produce a complete module observation. |

The current instruction debug safe-point hook runs **before immediate and
operand validation**. It is not a valid effect hook. `current_wasm_op_offset`
is a useful source offset, not validation completion.

The current `find_handler` returns SIZE_MAX both for no local match and
for a first retaining reference match. Do not treat that integer as proof
that a throw escapes unhandled. The Stage 1 `first_actual_handler` receives
the full original ordered snapshot and distinguishes consuming, retaining,
absent and invalid. Its successful same-function non-ref case adds no
escaping tag: that existing lowering already eliminates allocation/unwind
and must not be replaced by a native throw for this experiment.

## One instruction, one event

Introduce a stack-local, gated `native_eh_instruction_packet` reset at the
start of each real dispatcher iteration. It contains a closed event kind,
checked decoded target/tag, payload classification, a bounded temporary
handler snapshot and optional fragment CallBase witness. The validation
case populates it only after that case has checked the relevant operands;
the dispatch tail commits it only after emission has also succeeded.
The exception/control emitter does not independently advance the recorder.

For an ordinary numeric/control instruction use
`observe_scalar_or_control`. Supported classifications must be an explicit
whitelist of the already dispatched instruction and checked block/operand
types. An untyped numeric `select` and numeric-only locals can be included;
typed select/block signatures containing references, v128, any reference
instruction, throw_ref, GC/memory/table/global/atomic, unknown extended
opcode, helper or outgoing call disqualifies the function as a leaf.
Unreachable/polymorphic instructions still contribute their validated
effects; omitted dead IR cannot prove a body has no reference effects.

Use `observe_handlers`, `observe_throw`, `observe_call` or the final
`observe_outer_function_end` for their corresponding prepared events.
Each must consume the same checked next adjacent range exactly once.
In particular try_table's handler observation replaces its scalar event;
a throw's tag observation replaces its branch event. Calls with unsupported
routes still disqualify a leaf without manufacturing an eligible callsite.

The adapter never advances code_curr. Pointer comments for every new
pointer borrow/assignment must state its existing expression provenance:

```text
[code_begin ... validated instruction_begin ... code_curr] | code_end
[                    safe span                          ] | one-past
                                    ^ range end may equal code_end;
no read or pointer advance is performed by the observer.
```

Only after the actual parser has proved all pointers belong to the same
expression and `code_begin <= instruction_begin < code_curr <= code_end`
may their offsets be formed. Integer addresses or ordering of alien
pointers cannot establish provenance. The recorder checks adjacency and
length again using integers; it never reads the opcode bytes.

## Actual tags and ordered clauses

At admission build a bounded, compiler-owned local-tag observation table
from the initialized module's actual local_defined_tag_vec_storage.
For each entry, bound its index, borrow the genuine vector element, require
its real exception_identity and retain that exact owner. Keep the actual
record address for equality with the shared validator's resolver result.
This table is not writable by the guest, never accepts a caller-supplied
tag address and is destroyed with the compilation observation.

For a throw or tagged handler, first match the resolver's record identity
to an actual element in this table without dereferencing an unproved raw
identity. Then use the retained actual exception_identity. This proves
record membership; Stage 1's nonempty shared owner check alone only proves
lifetime. Two same-signature tags remain different. First-scope imported
aliases decline the module; future alias admission needs the trusted
provider/leaf ownership chain, not a resolved_tag pointer alone.

A handler snapshot iterates the live validation control stack from its
innermost frame outward, then each frame's original handler vector forward.
Check every target_frame is still a valid lexical outer target and map all
four catch forms exactly. Catch-all has no tag owner; tagged forms require
one. A matching ref clause is never skipped in favor of a later non-ref
clause. Copy the bounded observations before the source frame/handler
vectors can be popped, moved or resized. No borrowed clause span persists.

Do not use `llvm_jit_emit_state.control_stack.exception_handlers` as the
complete validated snapshot: `try_record...exception_handlers` returns
success without recording handlers for unreachable LLVM blocks. The
validation control stack is the authoritative already-decoded lexical data.

## Genuine direct-call witness and linking boundary

The global `emit_runtime_local_func_llvm_jit_may_throw_call` emits throw
bridges, imported calls, host/helper calls and cleanup routes as well as
ordinary calls. A hook there cannot establish ordinary direct-call origin.
Even call_ref with a known target can reuse the direct helper. Match the
prepared original ordinary opcode event and decoded target, actual local
function ownership, real callee declaration and exact FunctionType/calling
convention/argument layout at the final ordinary helper callback.
Unreachable calls produce no real CallBase and cannot acquire a witness.

The temporary CallBase pointer is valid only inside its actual owned
fragment while the builder is using it. Before recording a witness verify
its containing LLVM function/module and called operand identify the real
expected declaration; do not recognize it by a guessed native symbol name.
Store only the event ordinal, original byte offset, checked target and
observation callsite index in the Stage 2 result. Preserve exact invoke vs
call, normal/unwind successors, attributes and all operands unchanged.

Parallel fragments are serialized, parsed into a different context and
linked by `link_llvm_jit_module_fragments`; the original modules are reset.
A raw CallBase/Function pointer cannot survive that path. Stage 2's stored
witness means the original fragment did emit the ordinary call; it is not
proof that an optimized final linked call still exists. Stage 3 must add
trusted stable IR association and revalidate the final actual CallBase
after transformations before any redirect. Missing, cloned, inlined or
unmatched association declines the future redirect. Stage 2 adds no IR
metadata and never grants a redirect from its boolean witness.

## Completion, retries and bounded failure

Preallocate one optional output slot per checked local function before
scheduling workers; each existing non-overlapping task group writes only
its own slot. Do not resize shared vectors while a worker holds an element.
The module/instance owners and attempt domain outlive all joined workers.
Use one fresh domain for each actual compile attempt. The existing
prepare/pre-link/link/verify failures can restart serial emission; clear
all old slots/witnesses and allocate a fresh domain before that existing
restart. Never combine an earlier worker prefix with the serial retry.
Observer failure must not itself force a serial restart or second body pass.

Proposed initial limits, checked with overflow-safe arithmetic before
copy/allocation, are 256 functions, 64 local tags, 8 MiB total expression
bytes, 1,000,000 committed instructions, 4,096 retained ordinary callsites,
65,536 retained handler entries, 128 nested control frames and 256 clauses
per temporary ordered snapshot. Bound escaping observations per function
by the admitted actual local tag count. Over-budget bodies compile normally
with candidate recording declined. These are conservative experiment
limits, not language limits or a reason to reject valid Wasm.

For parallel recording, reserve quota for each retained logical entry before
copying it through a checked attempt-owned atomic ledger; include sidecars
and transient handler snapshots separately in a receipt. Refunded quota may
not make an invalidated prefix eligible again. These counts bound logical
payloads, not allocator rounding, vector spare capacity, cookies or RSS.
Either add an explicitly bounded-reserve allocator contract before claiming
a hard observer-byte bound, or report the real capacity/requested bytes and
measured peak separately. The keeper's 64 GiB cgroup remains the actual hard
process-memory bound. Never report entry counts as measured memory use.

Every observer-only allocation/copy runs inside a narrow exception boundary.
Catch only its own bad_alloc/record-copy failure, release its owners, record
decline and keep original IR/validation running. The existing validator and
LLVM exceptions remain outside that catch. A noexcept emitter callback may
only submit a non-allocating same-operation witness; allocating the handler
snapshot happens in the bounded adapter, not in that callback. Builds without
the required C++ exception/OOM contract decline this first experiment.

## Files and module exposure for the proposed patch

Add `translate/single_func_native_eh_leaf_observer.inc` for the private
adapter beside other single_func fragments, included inside the existing
compiler namespace. It is not a new validator or a guest-visible API.
Use the already supplied Stage 1 shared .h/.cppm from the exact-1 branch of
translate.h and the corresponding import in translate.cppm. The existing
compile_all_from_uwvm impl.h/impl.cppm already include/export translate;
they must expose the new gated result fields identically through both paths.
There is currently no shared/impl.h or shared/impl.cppm to invent or patch.

Small gated edits are limited to the exact hook files in the table plus
the paired translate header/module imports. Pure validator files, int/lazy
validation, r11/trace GC storage, the new setter, debugger and current
throw/trace bridges remain unchanged. Capture before-images and prove a
default-off text projection before handing the source to the keeper.
That receipt is textual only, not machine-code/performance evidence.

Stage 2 has no new native ABI and emits no new host symbol, instruction,
frame/poll, IR metadata, clone, nounwind or trace policy. Existing cache
objects are never permission to fabricate observations: the real current
fused traversal must record them again. A new executable code-generation
policy key is required only when the later private-copy stage actually
changes code; recording version/request still belongs in cold receipts.

Normal full currently calls `record_actual_full_validation` only in its
owned pending-plan branch, with another debug-only materialization branch.
Do not borrow either admission or set a fake full epoch in Stage 2.
Stage 3 must explicitly bind its own current actual complete normal-full
validation/source epoch and exact loaded code ranges before publication.

## Focused keeper cold tests for a later implementation

| Proposed test | Required actual witness |
| --- | --- |
| `test/0014.llvm_jit/llvm_native_eh_leaf_observer_fused.cc` | Real initialized source, the existing fused compile entry and actual successful local/module observations. Positive numeric leaf plus real ordinary caller edge; no direct recorder-only substitute. |
| Distinct equal-signature tags and nested clauses | Throw of the real second tag reaches its actual matching clause; inner catch_ref/catch_all_ref defeats an outer non-ref candidate; unrelated earlier tag does not. |
| Same-function numeric caught throw | Escaping tag count is zero and existing direct branch IR is unchanged; do not introduce native throwing to create a candidate. |
| Two escaping tags; caller covers one vs both | One uncovered tag declines that edge; all genuinely covered tags records a consumed effect, without changing native trace. |
| Missing outer end, trailing byte, bad later result/operand, malformed catch index/payload | No complete record despite earlier observed valid calls/throws; actual canonical validation error agrees with the oracle. |
| Reference block/select/local in dead code; throw_ref, GC, memory, table, atomic, tail/ref/indirect/imported call | Conservative leaf decline, unchanged validation/execution and musttail behavior; no known-target call_ref promoted to ordinary origin. |
| Unreachable ordinary call; debug/pending/lazy/routed/OSR/no-native-EH configuration | No genuine ordinary fragment-call witness, no complete qualified native scope manufactured. |
| Observer-only OOM, quota boundary/overflow and mid-stream invalidation | Original IR/module remains usable, no observer-triggered reparse, no partial completion. Existing validator/LLVM allocation failures keep their original contract. |
| Serial vs real worker groups; pre-link/link/verify retry | Same complete local effects under a fresh attempt domain; duplicate/missing/old-domain records are rejected; no LLVM pointer survives fragment destruction. |
| IR/object equivalence | In one initialized source and same real toolchain/options, compare request off/on under macro1: all original calls/attributes/landingpads/CFI/logical frames/trace bridges unchanged. Then real macro-off/on compiler source closure and disassembly; no new guest poll/frame/native symbol. |

Add valid and intentionally invalid Core 3 WAT fixtures under the existing
LLVM test fixtures directory; official validation and Wasmtime must consume
the same generated bytes as both products. A typed decoder rejection is not
a substitute for an intentionally invalid binary validator witness. Exercise
existing public uncaught/native throw_ref/cross-call/tag-alias and trap-stack
fixtures unchanged, not only private recorder units. No source-only fixture
in this plan is an actual native pass.

The keeper runs these small cold checks serially inside its exact 64 GiB
scope before any benchmark or Windows/native-EH expansion. Record observed
instructions, sealed functions, ordinary witnesses, consumed-effect edges,
declines, allocation requests/capacities and compile time. Stage 2 has no
runtime optimization: any difference in guest throughput must not be
reported as trace-capture improvement. Native private-copy/trace-bridge
benchmarks begin only after the separate source/publication proof exists.
