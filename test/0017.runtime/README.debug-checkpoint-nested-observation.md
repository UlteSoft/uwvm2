# Nested Core3 value observation producer (source proposal only)

This revision implements compiler-produced value observation at the actual
full-JIT instruction boundaries inside block/loop/if/else and try_table, including
normal child calls, saved if/else parameter values, catch_ref and throw_ref.
It does not claim executable restoration of those contexts or complete persistent
exception diagnostic traces. The resumable profile's active control/EH guards
remain; only the independently immutable observer profile selects this producer.

The original fused validator owns exact locals, operand and control declaration
types. A synchronous compiler-only callback runs where the original emitter has
selected the actual loop/else/end body block and real incoming PHIs. It borrows
that same walk; there is no preliminary validation or second body decoding.
The first loop at byte0 is distinct from the function entry at byte0. Function
end staging is committed only after the original result/trailing-byte checks.

Each lexical construct is recorded at its dispatch-proven entry. A compiler-only
forward-link map retains SIZE_MAX for its unresolved end; sealed_function_plan
rejects that value. The original validator's actual end transition resolves only
its own control and handler-continuation fields before LLVM verification/sealing.
The transient checker normalizes only a private copy of fields that match this
actual walk's pending scopes; no guessed source end enters the published plan.
No code address, native catch object, stop ticket or live execution owner is
created from this metadata. Failure rolls back both site cells and their links.

Control declarations are outer-to-inner. Active try_table clauses are ordered
inner-to-outer, preserving source clause order, linked tag indexes/identity-derived
payload declarations and the real target label. Loop handlers use the loop entry;
other handlers are backpatched to the real end. Original EH lowering already
copies caught values into owning SSA before end_catch and branches to the actual
PHIs, so observations use those owning values and current exnref carriers.

The packet contains locals, current operands, then saved if parameters. Waiting
and normal-return sites preserve this hidden suffix while consuming call args and
adding actual child results. Existing precise-root snapshots preserve saved if
reference values at every existing collection/call/stop boundary in the selected
observer profile; those values are not inferred from numeric bits.

Observation codegen policy changes from8 to12 in the canonical ten-field cache
identity. This source proposal deliberately preserves current resumable policy7
and tuple schema3. ROOT must narrow-compose retirement11/precall9/new resumable13
and the separate schema4 proposal, retaining the observer12 distinction. No
checkpoint file based on the earlier identity can be silently reused.

Default profile0 generates no new guest IR/bridge/packet/root stores. Existing
resumable nested/EH rejection remains honest. The public value query already reads
actual materialized locals/operands under the real cohort/host/root/publication
borrow. Dedicated control/handler presentation and whole-VM resumption are further
work; their metadata is now genuinely produced, not an authority substitute.

Primary references:
- https://webassembly.github.io/spec/core/valid/instructions.html (Core3 control,
  try_table, throw, throw_ref; exact tag payload and label rules)
- https://webassembly.github.io/spec/core/exec/instructions.html (loop/if labels,
  handler dispatch, owning exnref propagation)
- Local WAVM/Lib/LLVMJIT/EmitExceptions.cpp was read for native catch lifetime;
  its historical EH shape is not substituted for Core3 try_table semantics.

Qualification: no native compilation or execution has been performed on macOS.
All official wasm-tools parse/validate, actual LLVM-generated runtime tests,
verifier/assembly inspection and performance checks must be run by the sole
Linux keeper in the original64GiB cgroup using fresh matching runtime/main/host
objects. This proposal is SOURCE-only and contains no PASS claim for native code.

Finite test corpus:
- debug_checkpoint_observer_control_links.cc: DATA-only forward ends, byte0 scope
  collision, target fixup, pending seal rejection, zero executable continuation.
- checkpoint_nested_observation.wat: actual loop backedges, multivalue control,
  if saved values, nested child calls, all four Core3 catch clauses, catch_ref
  retained local and throw_ref rethrow (normal result316).
- checkpoint_saved_if_gc_observation.wat: struct hidden saved-if reference after
  operand consumption, subsequent allocation (normal result7).
- checkpoint_first_loop_observation.wat: real loop at byte0 (result42).
- checkpoint_identity_if_observation.wat: false no-else parameter/result identity
  merge (result42).
- debug_checkpoint_nested_observation_runtime.cc: real full observer configured
  before compilation, captures every actual instruction stop; management waits
  for the complete actual paused cohort and performs authenticated rooted typed
  locals/operand queries. Requires actual exn objects, nested calls and repeated
  loop source points for the modern EH fixture. Execute instruction and unwind
  for each fixture in each repository; the probe does not claim restoration.
