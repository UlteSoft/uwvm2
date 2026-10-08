This is a source candidate for a real captured-thread execution continuation in
LLVM-JIT full mode. It extends the real saved-leaf entry candidate with an
authoritative direct-call waiting packet and a native post-call landing. It is
not a complete-instance restore transaction, and no native result or performance
acceptance is claimed by this source artifact.

The same fused validator decodes the call immediate, checks the function type,
consumes its arguments and pushes the declared Core3 result tuple. Before original
LLVM lowering it supplies those exact post-call types and the checked next byte
offset to the compiler adapter. This adapter copies the real pre-call site,
removes the consumed arguments and stages an awaiting-return site plus its exact
after-call site. The opcode transaction owns the entire added range and rolls it
back if this original validation/lowering fails. It never scans the Wasm again.

The actual physical call preparer publishes the awaiting packet after popping
the arguments and before invoking the child. A real child activation therefore
enters beneath a materialized waiting caller in the shadow ledger. After ordinary
normal return, the same physical emitter installs the post-call PHIs and publishes
new result roots before updating the current logical packet. A call immediately
before the function's `end` has a real post-call landing too; structural `end`
observations are not used as fabricated executable authority.

The new host-only thread adapter authenticates both existing canonical capture
registries, requires the original monotonic participant to have actually retired,
and acquires one real runtime-generation lease. Before the first instruction it
checks every original module/source/engine/sealed-plan/function-generation owner,
resolves every entry through the actual complete full publication, prepares exact
host ABI outputs, and seals each typed caller-return projection against the actual
adjacent call sites. The original public leaf adapter still rejects multiple
frames instead of silently discarding their caller continuations.

Execution proceeds from the innermost saved frame outward. Only a real normal
return from a generation-pinned private native entry produces the child's packed
result bytes. Those bytes are copied into constructed typed slots using checked
actual ABI widths, then the existing typed projection constructs the caller's
after-call packet. The next private native entry jumps to its compiled post-call
landing, preserving the waiting prefix and skipping the already completed call.
No public returned tuple, DWARF label, native stack image, or saved numeric frame
identity can select or invoke an executable address.

The fixture uses Core3 nondefaultable `(ref i31)` locals, `ref.i31`/`i31.get_u`, an
i64 parameter and a live caller i64 prefix. Real original execution returns 1176;
two subsequent continuation executions must return 1176 while both prior root
and child prefix counters remain exactly one. A guest memory change after capture
must remain intact, making the lack of resource rollback explicit. The fixture
also checks premature original-stack use, foreign owner aliases, wrong result
extent, multiple-frame rejection by the leaf API and stale runtime generations.
Both products must run this actual fixture under instruction and unwind policies.
The root also executes a statically known `ref.func`/`call_ref` identity after its
ordinary direct call. The lexical waiting state is cleared at successful call
completion and each next opcode transaction, so this existing generic-call fast
path cannot accidentally reuse a previous opcode's waiting witness. This is a
positive legal-lowering regression case; it does not claim a new captured
`call_ref` continuation capability.

This first call slice supports exact-result direct local call relations beneath
the implicit function control. Active nested block/loop/if controls, active EH
handlers, subtype-widened child result declarations and GC/external reference
restoration need their actual continuations/root relocation adapters. Missing
capabilities are reported without rejecting otherwise legal guest Wasm. A
completed capture or typed projection never upgrades into whole-instance restore
permission. The separate complete-instance manager must still retire original
native stacks safely, prepare all resource/GC/thread state, validate cache/Wasm/
build identities, and atomically publish a fresh generation with newly issued
private continuation records. Old saved captures cannot authorize a new epoch.

Normal compilation has no selected plan and emits no call-site packet, resume
selector or additional execution callback. Selected recording currently does
materialize both waiting and returned packets, and must undergo actual reserved
cost benchmarking; source inspection is not evidence of zero overhead. Generated
resume code is still cache-disabled until every embedded legacy owner address is
replaced with an authenticated runtime binding. The call code-generation profile
revision is 7, so it cannot reuse revision-6/earlier cached IR.

The design follows the [Core3 function invocation and return rules](https://webassembly.github.io/spec/core/exec/instructions.html#function-calls):
arguments leave the caller stack before entering the child; the returned result
tuple is added back to the caller continuation. The [Core3 runtime structure](https://webassembly.github.io/spec/core/exec/runtime.html)
defines independent typed frames, locals, control labels and store instances.
The mapping into exact typed LLVM PHIs is an implementation design, not a feature
specified by Core3. LLVM's [LangRef](https://llvm.org/docs/LangRef.html) and bundled
LLVM23 `CloneFunctionInto` implementation govern cloning, real PHI verification,
native ABI layouts and preservation of genuine `musttail` transfers. Native
unwind remains the alternative diagnostic stack mechanism, not an extra
per-instruction bookkeeping layer or checkpoint resume authority.
