# Fused native EH observations: source candidate

Implemented as a source candidate on 2026-10-02. The exact-1 gate is
`UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER`; an explicit recording request
also defaults to false. No C++ build, encoder, oracle, guest execution,
assembly check or speedup is reported by this source receipt. The earlier
Stage 2 design remains a historical design, with its original hash.

The new `translate/single_func_native_eh_leaf_observer.inc` retains the
actual canonical full-source owner, initialized main module and bound
module ID. It authenticates function descriptors against genuine local
records and tags against retained original record/instance/control-block
identities. Its domain records one traversal only. It neither creates nor
reads a full-validation epoch as a permission; runtime callers without
this owner decline recording. No runtime publication or cache permission
is added.

All eight existing compiler files use exact-1 blocks. Their gate-off text
projection is byte equal to each repository's captured before-image,
including the original direct-call callback in its `#else`. This is a text
fact, not a preprocessor or assembly result. The independent r11/precise
trace GC heads, scalar struct setter and Stage 1 effect header keep their
frozen hashes. Concurrent debugger work in shared files is a distinct
later source snapshot and must be bound separately for native tests.

The existing fused dispatcher starts a packet at its checked opcode,
prepares actual validated tag/target/control data, and commits only after
the original instruction's validation and emission succeed. The outer
function `end` commits after result/trailing-byte checks and LLVM
finalization, before its existing internal return. Actual handler snapshots
come from the validation stack, including unreachable frames, inner first
and with each vector's original clause order. Catch-ref, unknown/reference
effects and unreviewed helper opcodes disqualify leaf selection; they do
not reject valid Wasm or disable LLVM emission.

An ordinary-call witness requires original opcode `call`, the same actual
module and target, and a real emitted CallBase. Same-operation pointers
are cleared before the next opcode. Post-success commit checks the actual
callee/caller module, direct target, FunctionType, argument types and the
completed calling convention. Persistent witnesses contain only bounded
integers. A dead call or call-ref route cannot manufacture a witness.

Records transfer only after the function's fused entry returns success.
Module closure follows worker joins, original linking and LLVM
verification. An existing serial retry begins a fresh domain and drops
old fragment records. Observation allocation/quota failures drop only the
observer; they never reset IR, cause a compile retry or change an exception
call. Local metadata retains copyable shared ownership so the existing
lazy descriptor-copy contract remains intact, though recording admits full
mode only.

The budget is fixed at at most 256 functions, 64 local tags, 8 MiB total
expression bytes, 128 control frames, 256 current handler clauses, one
million completed events, 4096 witnessed calls and 65536 retained call
clauses. Call quotas use overflow-checked atomics across worker fragments;
failed claims are not refunded. Tags/escaping sets are further bounded by
the genuine admitted tag/function extents. Own allocation catches surround
only observation allocation/copy work. There is no second body scan.

`llvm_native_eh_leaf_observer_fused.cc` loads official-encoded fixtures into
an actual selected, parsed and initialized owning source, binds its actual
main module, and invokes the real all-function compiler with serial and
two-worker splits. The proposed cold checks compare recording off/on IR
bytes, missing-owner and zero-budget fallback, catch order, unreachable
calls, a late invalid function and a restored attempt slot. The validation
epoch must stay zero throughout. The valid WAT includes both local and
cross-function Core 3 `try_table`/throw/catch/catch_ref syntax; the invalid
WAT has a later stack-invalid result so a valid prefix cannot seal a
module. The identical encoded bytes must also go through official and
Wasmtime validation on Linux before any native result is accepted.

This candidate observes source/IR facts only. It implements no private
clone, `nounwind`, trace omission, altered public ABI, trace bridge or
executable selection. Existing throw, cleanup, CFI, stack/frame and
uncaught-error behavior remains the original path. A later optimization
must separately prove loaded-code/generation/root ownership and measure
its costs; completed observations alone authorize nothing.
