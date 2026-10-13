# Actual checkpoint-selected native effects

`fixtures/debug_checkpoint_host_effects.wat` uses Core 3 GC struct types,
nondefaultable locals, `call_ref`, `return_call_ref`, direct calls, indirect
calls and all three tail-call forms. A real native local provider synchronously
reenters the canonical LLVM-full instance through the public raw host API.
The native callback's reentered function has an uninitialized nonnullable GC
local; its stop must keep the original index and report it unavailable.

Run only after the effects bridges and private retained host-operation producer
are integrated into a fresh matching runtime/main/host build. This test must
not link an older runtime object, mix frozen source cuts or count codec-only
checks as runtime acceptance. The current independent test source is pending
that integration and native execution.

The official `wasm-tools parse` command assembles the WAT fixture remotely. Its
default validator accepts the Core 3 feature syntax; preserve assembler and
validator versions and stdout/stderr in the evidence. Build the C++ test with
the exact same LLVM/fast_io/provider/macros as the current full runtime. Link
the matching fresh runtime, entry and host objects in their recorded order.
Then run these twelve cases per product in the existing 64 GiB test cgroup:

```text
debug_checkpoint_host_effects_runtime fixture.wasm instruction 1
debug_checkpoint_host_effects_runtime fixture.wasm instruction 2
debug_checkpoint_host_effects_runtime fixture.wasm instruction 3
debug_checkpoint_host_effects_runtime fixture.wasm instruction 4
debug_checkpoint_host_effects_runtime fixture.wasm instruction 5
debug_checkpoint_host_effects_runtime fixture.wasm instruction 6
debug_checkpoint_host_effects_runtime fixture.wasm unwind 1
debug_checkpoint_host_effects_runtime fixture.wasm unwind 2
debug_checkpoint_host_effects_runtime fixture.wasm unwind 3
debug_checkpoint_host_effects_runtime fixture.wasm unwind 4
debug_checkpoint_host_effects_runtime fixture.wasm unwind 5
debug_checkpoint_host_effects_runtime fixture.wasm unwind 6
```

Indices 1/2/3 exercise direct/indirect/reference calls and must return 42 after
one real native effect. Indices 4/5/6 exercise their tail-call equivalents and
must return the provider's 0 after one effect. At the real stop inside the
provider callback, recording must already have `non_replayable_import`, the
native provider must not yet have returned, and the uninitialized local's
payload must never be read. Host-tail retirement must not fabricate a caller
frame to make this observation appear complete. Check the selected LLVM IR for
the correct bridge and musttail transfer before platform-specific assembly.

The test never requests save, restore or replay. A native operation remains
suspended on the callback's native stack even while the enrolled guest is
parked. A cooperative pause and an effects status cannot authorize a coherent
worldstop, protected asset publication, a GC census or external-world rollback.
Unknown native effects remain explicitly non-replayable until a real replay
adapter owns their input/result bytes, resource lifetime, ordering and side
effect policy. See the [Core 3 call execution rules](https://webassembly.github.io/spec/core/exec/instructions.html#function-instructions)
and [QEMU replay contract](https://www.qemu.org/docs/master/system/replay.html).
