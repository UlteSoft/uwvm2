# Private native-EH leaf observations: first-stage component

Status on 2026-10-02: this is a source implementation and standalone component
test, **not a compiled, executed, benchmarked or platform-qualified change**.
Neither repository imports the new header/module from a production `impl.h` or
`impl.cppm`. No emitter, LLVM IR, runtime bridge, object cache, materializer,
debugger, hot replacement or canonical validator behavior changes.

The previous `llvm_native_eh_private_leaf_design_20261002.md` remains a design,
including the later source ownership, ABI, loaded-range, debug and cache work.
Its future clone optimization is not implemented by this first stage.

## Scope and API

The source closure is
`src/uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h`, its
standalone `.cppm` adapter, and
`test/0017.runtime/wasm_exception_private_leaf_effect.cc`. The header and test
use the C++23 standard library only. The module adapter is independently
available but deliberately absent from production module imports.

`observation_domain::create(module_index, function_count)` creates an immutable
native lifetime domain for one traversal. It rejects empty/out-of-extent counts
and the unknown module index. `has_actual_observation_owner` rejects a nonowning
alias and verifies the object's own `enable_shared_from_this` control block.
This is an observation lifetime, **not** an initialized VM module, source pin,
runtime generation, full-validation epoch, pause ticket or native permission.
Numbers which happen to equal another traversal's module/function indices do
not give its records a shared domain.

`linked_tag_identity::observe_actual_linked_instance` retains an opaque owning
pointer to an already authenticated linked tag instance. It never dereferences
the tag pointee. Identity compares the retained actual address rather than a tag
index, signature or payload. This preserves imported aliases. Empty and
nonowning pointers fail, but nonempty ownership alone cannot authenticate a
fabricated native alias or guest address. The future trusted adapter must first
authenticate the live initialized record and obtain its actual retained
`exception_identity`; no guest integer-to-pointer conversion is allowed.

`function_recorder::begin` accepts expression size, function index and the
already validated numeric/non-numeric/unknown signature classification.
`observe_*` methods receive adjacent `[begin,end)` byte **offsets** for events
from the existing fused validator/emitter. Offsets are checked against the
recorded extent; no body byte is read or parsed. Gap, overlap, reversed, empty,
out-of-bounds or post-end events poison the recorder. Only an exact outer-end
event reaching the extent can seal. Sealing consumes the private owner/vectors,
is move-only and cannot be repeated.

The event classifications are conservative:

- Scalar/control events alone do not disqualify a numeric leaf. The adapter
  must distinguish these from every effectful opcode; a byte interval is never
  a substitute for decoding and validating its real opcode and immediate.
- Reference/vector/unknown signatures or throw payloads decline leaf use.
- Ref handlers, `throw_ref`, tail transfers, memory/table/global accesses,
  host/unknown effects and unknown opcodes decline leaf use.
- Every outgoing call declines the function as a leaf. Ordinary local direct
  callsites can still be recorded in the **caller** for later analysis.
  Tail, indirect/ref, imported/host, unknown or unknown-target routes never
  create such a callsite.
- A numeric throw consumed by the existing same-function non-ref handler has
  no escaping effect. It keeps the already implemented direct-branch path.
  Otherwise its actual tag instance is retained and deduplicated by identity.
- Allocation failure when recording a throw/callsite permanently poisons the
  recorder before rethrowing the C++ allocation exception. A host which catches
  that exception cannot later seal a summary missing the advanced event.

Handler input is an already validated snapshot ordered inner-to-outer and then
clause-first-to-last. Every clause is shape-checked before matching, including
clauses after an apparent first match. Snapshot owners/order are copied for a
direct callsite; no span or pointer into mutable decoder/control-stack storage
escapes. The first matching actual tag instance or catch-all determines whether
the exception is consumed or retained. An inner `catch_ref` for an imported
alias therefore prevents selection even if a later outer plain catch uses a
different numeric tag index for the same instance. An unrelated inner ref
catch can be skipped when its actual instance is proven distinct.

`observe_consumed_direct_call(caller, callsite_index, callee)` returns an analysis
enum only. It requires complete observations from the same genuine traversal
domain, exact local target index, a qualifying numeric leaf, at least one
escaping tag, and a first non-ref consuming handler for **every** escaping
instance. It never issues an executable address, publication token or native
admission. The callee has no host/outgoing calls in this initial classification,
and its payload cannot contain an old exnref.

## Required future binding

The component cannot authenticate that a host supplied the true opcode, tag,
signature, lexical ordering or function body. Its sealed observation is not a
canonical validation proof. The existing fused pass remains the only decoder;
events are emitted after each real opcode is validated. Full compilation must
succeed before consuming any completed summary. No whole-body prevalidation,
second decode or extra validation walk is introduced here.

Before redirecting an IR edge, a future adapter must bind exactly one summary
per original function to the same initialized owning source and actual full
validation epoch, actual LLVM function/ABI, actual linked tags and module
generation. It must decline partial compilation, duplicate/stale bindings,
unknown routes, unsupported modes/platforms, armed debug/session attachment and
replacement-capable state. An observation domain is insufficient for any of
these checks. Each loaded clone extent must be authenticated and mapped to the
original Wasm function identity before executable publication. Public,
exported, table/ref and original function entries retain complete original
trace behavior. Forced native dispatch must still use genuine native C++
raise/cleanup, rather than silently becoming pending numeric dispatch.

No cache schema/key, clone range, publication capability, trace-free bridge or
runtime source ownership is introduced in this stage. Old exnrefs and
cross-module/import aliases remain subject to their existing runtime lifetime
and trace rules.

## Standalone counterexamples and remote verification

The test uses real C++ owning lifetimes as opaque **analysis** observations; it
does not manufacture a VM tag record or claim runtime membership. It checks:

- Plain consuming direct call; alias identity; two escaping tags with complete
  coverage; an unrelated ref clause before a matching plain clause.
- Ref/all-ref shadowing, including a retained alias with the same tag instance;
  same payload in a distinct instance; absent handlers and missing second tag.
- Duplicate escaping aliases; owned immutable callsite snapshot after the
  caller's original vector is mutated and cleared; different traversal domains
  with equal indices; wrong/out-of-bounds local targets and callsite indices.
- Non-numeric/unknown signature and payload, every disqualifying effect,
  outgoing calls, a ref-handling leaf, non-direct routes and unknown targets.
- Already consumed local throws (no clone opportunity), malformed/late unknown
  handler observations, empty/nonowning owners, incomplete/gapped/overlapping/
  reversed/out-of-range/post-end streams and repeated/moved-from sealing.

The optional `UWVM2TEST_PRIVATE_LEAF_OOM=1` test variant replaces only the
executable's ordinary C++ allocator. It fails the next actual allocation for
the escaping-tag vector or copied handler vector, catches `std::bad_alloc`,
and checks that the advanced partial recorder cannot continue/seal. Its C
allocator declarations are explicit `noexcept` assembly-linked host symbols;
the GNU/Clang variant uses the correct Mach-O underscore where applicable.
This injection is not linked into the runtime or exposed as a product API.

Suggested keeper commands, entirely within the existing remote constrained
cgroup and using the already qualified host C++23 toolchain:

```sh
clang++ -std=c++23 -O3 test/0017.runtime/wasm_exception_private_leaf_effect.cc -o private-leaf-effect
./private-leaf-effect
clang++ -std=c++23 -O3 -DUWVM2TEST_PRIVATE_LEAF_OOM=1 test/0017.runtime/wasm_exception_private_leaf_effect.cc -o private-leaf-effect-oom
./private-leaf-effect-oom
```

Successful component runs exit zero and emit no output. They test the event
analysis contract, not Wasm validation, native EH behavior, throughput or any
platform unwind backend. Real callee-to-caller native/ref trace fixtures and
cache/debug/replacement/platform witnesses remain required before later IR
integration can be called implemented or qualified. No local compilation or
execution was performed for this stage.
