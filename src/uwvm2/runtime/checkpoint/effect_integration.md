# Checkpoint-selected import effects: source integration proposal 4

This proposal adds two native bridges selected only while compiling an engine
with its retained immutable checkpoint profile. The independent implementation
is `runtime/lib/uwvm_runtime_checkpoint_effect_hooks.h`. It is not yet included
by the runtime TU or selected by the shared compiler. No native acceptance or
whole-state checkpoint capability follows from this source candidate.

## Actual target classification

The current runtime already resolves imports into `cached_import_target` final
leaves. Its `defined` kind is a Wasm function, including a cross-module import
alias. `local_imported`, `dl` and `weak_symbol` kinds enter native providers.
Classification uses the actual owned cache member, canonical full source
registry, live engine/context and the exact profile pointer AND control-block
owner. It does not walk an untrusted import chain or grant a capability from a
module name, supplied enum, native address or public boolean.

The new direct bridge resolves a comparison-only module address through the
actual runtime registry before reading that module. It checks both cache
dimensions, original module identity and the final leaf's parameter/result byte
extents. A defined leaf must also match the actual provider's local function
storage and checkpoint-selected canonical full publication. Native leaves have
no completed record/replay adapters yet and poison recording with the sticky
`non_replayable_import` result BEFORE executing the provider. Normal provider
execution, exceptions and host reentry retain their existing runtime semantics.

The dynamic bridge checks the real cached-import entry symbol, then checks the
context against actual cache allocation bounds and exact complete-record
alignment before indexing any member. Integer overflow, interior pointers and
one-past addresses are rejected without dereferencing the supplied context.
This examines one range per module instead of scanning every imported function
on each native raw call. A direct
full raw entry must match a real current full publication's raw entry and have
the eager wrapper's zero context. This current-address comparison is an effects
classification; it is never permission to execute a pointer read from a file.
An unknown/retained/lazy/native-provider entry conservatively declines replay.
Defined Wasm targets preserve the actual Wasm caller activation instead of
adding a native host island. Native targets retain the existing qualified raw
forwarding and debug host scope. Raw SysV/fastcall conventions remain paired
with the actual runtime entry type.

A retired tail caller can have no current top activation. Its selected native
adapter still inherits the genuine generated-depth token and scope-owned shadow
ledger. The effect marker does not require a fabricated frame in that case and
does not mint a new continuation. Existing typed-tail pending-continuation checks
remain responsible for actual successor entry. This source slice does not supply
the missing complete call/return/EH/PHI materialization.

## Exact shared edits proposed, not applied

| File | Proposed narrow region | Responsible owner |
| --- | --- | --- |
| `runtime/lib/uwvm_runtime_generated_wasm_bridge.h` | Append two checkpoint-only six-argument integer ABI declarations after checkpoint activation declarations. Existing declarations/packet ABI unchanged. | Checkpoint, root review |
| `runtime/lib/uwvm_runtime.default.cpp` | Include the new effects leaf after `llvm_jit_call_raw_from_generated_wasm_abi_bridge` is defined; all cache/raw target/canonical ownership helpers are then visible. | Checkpoint, root review |
| `translate/single_func_emit.h` | Direct imported raw call: nonnull actual `state.checkpoint_plan` with its retained immutable profile selects the new direct bridge symbol. Otherwise select the original symbol with identical arguments/IR. | Checkpoint, compiler owner review |
| `translate/single_func_emit.h` | Direct imported host-tail adapter: select the same new bridge at compile time before the actual adapter call; do not insert work between musttail and its required return. | Checkpoint, compiler owner review |
| `translate/single_func_emit.h` | Observational raw target helper and indirect host-tail adapter: checkpoint profile selects the new dynamic bridge; ordinary debug selects the existing debug wrapper. | Checkpoint, compiler owner review |
| `translate/single_func_debug_host_bridge.h` | Exact-function-pointer control classification of the two new bridges, or use the existing explicit unwrapped-symbol path. A linked Wasm leaf must not acquire a host island before actual classification. | Checkpoint with Wasm debugger owner |
| Runtime private host entry/provider scope | Count real native operations before borrowing or participant setup; cover host reentry/escaped native paths, retain gate owner and release on normal or exceptional exit. | Host admission owner |

The source read on 2026-10-03 has the direct raw imported call near emitter line
9404, dynamic raw target near 9322, direct host-tail adapter near 10157 and dynamic
host-tail adapter near 10177. The current raw ABI implementation is near runtime
line 21105. Exact anchors and current before-image hashes, rather than old line
numbers or entire-leaf copying, must govern the later patch.

The effects helper supplies classification, not a private host-operation token.
Its non-defined forwarding branches now construct the separate private
`runtime_checkpoint_host_bridge::foreign_operation_scope` supplied by the host
admission owner and require `admitted()` before invoking a provider. That scope
uses only the actual retained entry, real generation lease and matching native
profile/control owners; no enum or address from effects classification grants
it admission. Its RAII operation persists through real native return and C++
exception unwind. The direct native branch also enters the existing genuine
debug host scope after actual target classification and admission. The new
bridge is an internal control symbol, so a defined Wasm target keeps its caller
activation; an actual native callback creates the foreign-host island used to
mark a reentered guest stack incomplete. The dynamic native branch retains the
original debug raw-target wrapper, which already owns that scope. Neither
source leaf is yet included by the runtime TU, and
the actual external entry/participant setup hooks remain a separate shared
integration obligation. A `defined` linked Wasm leaf does not enter that host
gate. Unknown foreign callbacks and escaped direct
memory views require explicit admission failure, not a successful census.
The gate's nonwaiting closed token proves only its counted host operations;
it does not prove all guest participants parked, canonical owners pinned,
external resources virtualized or a complete FD/preopen/plugin census.

## Default execution and qualification

The new source leaf is presently unreferenced. The eventual compiler selection
is a compile-time retained policy choice; default full, observation-only debug,
lazy, interpreter and tiered code must gain no new generated call, branch, TLS
load or guard. Ordinary host APIs must not receive a generic checkpoint probe.
The checkpoint cache policy revision must change when these selected symbols
change. Fresh runtime/main/host TUs, all generated ABI providers and current
LLVM object-cache keys are required together; old ABI qualification cannot be
borrowed.

Actual tests must call native imports through direct call, call_indirect,
call_ref and all three return_call variants, with normal return, host reentry
and provider exception. Checkpoint observation after an unadapted native effect
must show the explicit sticky result; original Wasm result and exact native
provider call count must remain correct. A Wasm import alias is a control case
that must not be mislabeled as a host effect, though incomplete caller
materialization can still make restore unavailable for its independent reason.
Actual emitted default IR must be compared to the unselected source policy.

## Replay and external state

[Core3 function invocation](https://webassembly.github.io/spec/core/exec/instructions.html)
distinguishes defined function evaluation from host invocation. A native host
function can modify the store and supply host-dependent results. Its type/ABI
alone cannot establish deterministic behavior. The bridge therefore cannot
accept a caller claim that an arbitrary provider is replayable.

[QEMU's replay contract](https://www.qemu.org/docs/master/system/replay.html)
records nondeterministic inputs and device events and requires an initial
machine state. For UWVM, a future admitted provider adapter must similarly own
its exact input/result bytes, effect ordering, external-resource lifecycle and
side-effect policy under the real replay manager. Entry-state recording plus
software instruction/source trace does not meet that contract. Current codec
event data and host-operation admission do not implement those adapters.
