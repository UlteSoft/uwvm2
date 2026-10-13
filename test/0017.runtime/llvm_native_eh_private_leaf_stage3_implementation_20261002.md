# Private numeric EH leaf: first source implementation

2026-10-02. This separate default-off candidate implements actual call metadata,
an independently owned staged IR transform and a versioned no-trace bridge. It
has not been compiled or executed. The normal publisher does not read the staged
field: no private native code, CFI registration, executable permission or speedup
is claimed. The earlier Stage2 v2 source snapshot and r11/GC/setter snapshots
remain unchanged.

`UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF` must equal 1, with the separate default
false `compile_option::stage_native_eh_private_leaf`. Actual complete Stage2
observations are also required. The metadata is attached to the real ordinary
CallBase only after the original fused instruction has validated and emitted
successfully. Six fixed i64 fields carry version 1 and five checked integers;
they carry no IR address or executable permission. Own allocation failure only
declines staging and never resets original IR or starts another validation pass.

The private factory is callable only by the genuine full-fusion entry after
original success, worker joins, link, verification and observation closure.
It retains that real initialized source attempt and actual tag control blocks.
It serializes the complete original module into a separate LLVM context, matches
each call exactly once against its retained witness, then checks the final public
callee, type, calling convention and arguments. Original pre-link callbacks
decline this first candidate. The effect selector checks every escaping actual
tag against the original inner-first ordered handlers; a retaining first handler
blocks selection. This is a private prepublication provenance owner, not a full
validation epoch or a published-code certificate.

Each selected scalar leaf has a separate internal clone. The original public
leaf and original caller remain intact. Clone cleanup, personality, UWTable,
typed ABI and native raising remain intact; no `nounwind` is added. Only an exact
numeric bridge identified by its type-discriminated symbol and actual registered
host address is replaced. The replacement has four `uintptr_t` inputs, copies
the same checked native tuple, retains payload/tag/source ownership and invokes
the original native `throw_value`. Only diagnostic trace capture becomes empty.
No guest bit or runtime guess grants permission. RV64's existing indirect bridge
materializer is deliberately not authenticated by this initial matcher, so that
target must decline staging while original full compilation succeeds.

The actual fused cold fixture uses the same official-encoded valid/late-invalid
bytes as Stage2 v2. Serial and two-worker paths retain its owning loader,
initializer and strict actual late `end_result_mismatch` checks. On admitted
targets it inspects one selected edge, one clone and two private raises, the
independent context, unchanged public raises/call, preserved native attributes
and three explicitly false loaded-code/runtime flags. Metadata format negatives
operate on a genuine fused call. Missing actual source and exhausted observer
budget decline observation/staging without changing original compilation.
These are source-level intended checks; there is no native pass yet.

Default-off receipts are textual only. Four compiler files project byte-for-byte
to their captured actual before-images. Two runtime files also received the
debugger agent's independent host-wrapper correction: its genuine before equals
the Stage3 before, and current Stage3-off equals the genuine debugger after with
Stage3 removed. Both real byte sets and original manifests are retained. This
decomposition does not claim that the entire runtime file was unchanged.

The frozen source includes current real debug dependencies to avoid mixing a
new cleanup implementation with an old emit-state layout. Keeper must reconstruct
that exact snapshot, record the complete actual source/dependency fingerprint,
and rebuild macro-aligned runtime and fixture inside the current 64GiB, swap-zero
scope. No historical init PID or cgroup is reused. Undefined/0/2 and observer-off
imports are compile-only controls; exact-1 actual fused cold and sanitizers come
before any executable publisher experiment.

Next production slice is the real publisher binding, after this source review:
retained actual source/attempt under its existing publication lock; current
runtime generation; finalized object/native engine; actual private function code
extents and native CFI; logical-PC mappings; outer execution admission and drain.
Object-cache replay and debug/replacement management initially decline private
selection. The staged owner must not be presented as those missing permissions.
Unprofiled guest/wall/CPU/frequency, whole-guest pure hardware counts and VTune
hardware samples remain three separate measurement families.
