# GC member mutation — private source qualification plan

`set wasm member THREAD ROOT [path original_indices...] at MEMBER SOURCE`
accepts original locals/operands/saved, globals MODULE INDEX, table MODULE INDEX
ELEMENT, or current session-local handle SESSION HANDLE. Source grammar is the
existing bits/null/i31/function/from grammar. Target and source handles are DATA:
the controller resolves immutable indices against the exact complete current
stop, then the runtime reborrows both original roots under execution/ONE/cohort,
tracked host close, actual exclusive N and source/publication/generation guards.
Success advances the real stop ID and clears the path ledger. Native/ASM stops
cannot substitute for the genuine cooperative capture.

Struct field and array element writes authenticate a local registered object,
real owner/controlblock and canonical type/layout, index and mutable declaration.
References additionally authenticate canonical source types and retain foreign
arenas in BOTH module roots and the actual object's value_leases. The latter is
required because an exported object can outlive its receiving module. Compact
numeric, exception payload and external-wrapper fields remain immutable. No
ordinary foreign compact reader/setter is called while exclusive N is held.

Protocol2 member replies report actual owner module and canonical type index,
plus the original member index. They never return an object token/host address.

All tests require fresh source builds in the existing SSH Linux64GiB cgroup;
no Mac/native or Python tests were executed while preparing this SOURCE packet.
First compile/run standalone debug_wasm_mutation_data.cc and the bounded DAP
DATA test. The DATA binary accepts --require-big only on an actual big-endian
QEMU target. Then validate the main/provider WAT with official contemporary
Core3 WAT tooling and build the actual runtime fixture against the EXACT paired
source snapshot; instruction/unwind strategies in both products. The runtime
fixture creates actual main-owned mutable scalar/vector/reference structs,
foreign compact reference arrays and packed i16 arrays, then obtains two real
generated before-park captures. It checks packed truncation, raw NaNs/v128,
exn/extern/typedfunc/i31/foreign compact sources, restore of self-cycle and array
baselines, immutable/OOB/type mismatch/incomplete cohort/extra-reader refusal,
and a genuine4096-edge original target after fresh query-qualified DATA ledger.

This is not a benchmark or foreign-only reclamation proof. It does not prove
controller handle integration by replacing the real controller with a fake.
No debug write is promoted into an authenticated deterministic replay event,
external host effects, source lifetime certificate or restore capability.

Core3 store semantics reference:
https://webassembly.github.io/spec/core/exec/instructions.html#exec-struct-set
https://webassembly.github.io/spec/core/exec/instructions.html#exec-array-set
