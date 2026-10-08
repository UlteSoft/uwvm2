# R6 immutable ring: one bounded table-get/cast candidate

This is a source proposal, with no production edit or native candidate result.
The Ordinary R6 ELF is13e649c7eb6b77100b8c9f7b41a4a37c76f29bfdf7c99c0422c0bfd92a4d4a82;
its original191-byte input is66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e.
The independent capture followup f37b2823 closes the successful two captures;
the original14-stage group remains false because the full host readobj exceeded
its existing output limit. The source proposal is not a P0 result.

Actual func0 machine code retains four calls on the successful loop. Unwind call
offsets are0x9d,0xd2,0xe8,0x102; instruction offsets are0xad,0xe2,0xf8,0x112.
Source informs the allocation/table-set/table-get/cast interpretation, but the
bridge-hash-to-actual-host-address binding still needs the matching helper audit.
The normal cast witness>1 already feeds one `movl (%rax)` field load; witness1
uses the existing checked getter fallback. Allocation output is copied as a full
16-byte reference into table-set input. Existing finalization already erased the
empty GC root frame. Neither the direct field load nor empty-frame deletion is a
new optimization.

In matching R6 source, table-get's GC branch transfers retention to the caller's
store before copying the complete reference. Its ordinary-local success uses
`retain_gc_reference`→`checked_object`→`checked_local_object`. The following cast
uses `native_immutable_numeric_struct_cast_values` and performs another local
membership query before canonical subtype/prefix/layout checks. Table-set retains
against the actual table owner's store; it is an independent third source route.
The actual local bucket walk and the registration of the two emitted bridge
symbols must survive optimization before calling this repeated work measured.

The narrow candidate combines only a successfully validated adjacent
`table.get`→`ref.cast` for an eligible immutable numeric struct target. It leaves
table-set, allocation, the direct field load and collector algorithm unchanged.
It changes neither table contents nor the Wasm reference representation.

```text
actual table/bounds → one actual slot read → original caller-retention lookup
                                    │
                   ordinary local struct, actual owner==caller store
                                    │
                   same C++ operation: canonical subtype + prefix checks
                                    │
                   full reference output + existing cast witness0/1/>1

foreign / array / compact / null / other kinds → original cast route
```

The runtime implementation would be a private store operation, provisionally
`retain_then_immutable_struct_cast_values`. Its original retention lookup can
return a private ordinary-local node only when that lookup actually succeeded
and its real owner is this store. It may reuse that checked node inside the same
operation. It must not accept a caller-supplied object pointer, authenticate an
opaque token by dereferencing it, export a reusable authorization bool, or infer
origin from a declared type. The node is not passed across a second bridge call.
Only the already supported immediate immutable field witness may leave, with its
current consume-before-any-call/poll/mutation/control-change contract.

Keep the original store-valid/kind/null/lookup and retention-failure order.
On local miss or every foreign/compact/unsupported route, use the old cast
implementation after the old retention, without an additional probe or fewer
foreign lookups. Foreign weak promotion, lease retention, canonical identity,
cohort membership and provider lifetime therefore keep their existing protocol.
For local reuse, still run expected-type validation, canonical subtype, actual
layout, field-prefix length/storage/immutability and initialized payload bounds.
Do not use shape equality as source or ownership authority.

The emitter must decode and validate the cast exactly once in the existing fused
loop before changing emitted IR. No future-immediate peek or body prevalidation
is added. Record the prior actual table-get call and output within this compiler
operation; after cast validation, replace it only if the entire original interval
and all uses satisfy the rule. Unsupported or failed recognition leaves the
original call/IR intact; it cannot emit a second table read as fallback.

Table-get OOB and retention failures stay at the original get site and precede
any cast failure. A failed cast is returned as witness0 and trapped at the original
cast instruction. Nullable null remains witness1, preserving any later null
field-get trap. The complete reference stays available wherever the original
operand was required; a witness cannot replace a Wasm root/reference.

Admission requires the real local-defined table and executing caller store,
validated target definition and owned initialized full-compilation source.
No call, allocation, safepoint, poll, root publication, control merge, EH boundary,
trace position boundary or observer may intervene. Debug/checkpoint per-op
selection declines fusion. The original actual managed entry/lifetime exclusion
still protects the lookup; shared admission does not prove a single mutator.
There is no table-set→get forwarding, cached token across iterations, lease skip,
mutable-field read without its lock, or direct guest-token load.

Before implementation, choose this candidate only if matching actual host
assembly shows both successful local lookups and P0 hardware/profile evidence
supports the cost. If host optimization already removes a lookup, or allocation/
collection dominates, preserve that finding and choose one different change.
The separate full16-byte copy may be a smaller emitter candidate, but it is not
combined into this A/B and cannot be called elimination of authentication.

An implementation needs a separately reviewed exact1 default-off profile and
versioned register-wide bridge/cache identity. Default undefined/0/2 preserves
the prior path. All three product TUs must use one macro vector and new actual
source/compiler/MD/SDK/provider closure. A later common-source baseline/candidate
A/B uses fresh products; old R6 and a newer GC31/EH44 source are not equal-source
controls simply because the input is unchanged.

The first correctness cells should cover local success, same canonical subtype,
wrong cast type, OOB-versus-cast precedence, nullable null and null field trap,
foreign object in local table with provider retirement, imported table refusal,
stale/forged token, unrelated live store and actual root-drop collection. Check
original trap class/site, complete carrier, retained foreign owner, exact checksum
and failed full-graph zero-reclaim semantics. Reuse current tests/fixtures; no
new guardian or broad matrix is required before these controls.

Capture candidate/baseline objects only in untimed cells. Confirm four calls
become three on the chosen local path, one authenticated lookup replaces two,
existing canonical/layout checks remain, no new runtime guard/call/poll appears
and the single field load/root publication/trap paths are preserved. Then run
the same191B16M input with finite reverse-order plain pairs, separate whole-guest
cycles/instructions with exact enabled==running, and independent hardware VTune
Hotspots/uarch. Internal Wasm time includes allocation/table/cast/checksum/GC; it
is not collector latency. Preserve actual frequency/MUX and unknown JIT mapping.

For context, pinned Wasmtime49.0.1 copying lowering uses inline bump allocation
and stack-map roots with a moving stop-the-world collector. Its barrier choices
follow that collector contract; they do not authorize removing uwvm's foreign
owner authentication or root/cohort exclusion.
[Official pinned copying lowering](https://raw.githubusercontent.com/bytecodealliance/wasmtime/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs)
