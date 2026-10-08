# Same-walk current-opcode typed producer proposal, revision 3

This source proposal is not applied to the production compiler/runtime. It connects one real, opt-in LLVM-full producer: exact semantic pre-op types from the authoritative fused validator, actual typed native payload and executed local-initialization flags, canonical current-frame publication checks, and synchronous DATA commit before the genuine cooperative debug park. It does not provide instance save/restore, reverse execution, deterministic replay, a protected database publication capability, complete root registration or an executable resumed-child/whole-function dispatcher.

## Standards and type source

[Core 3 instruction validation](https://webassembly.github.io/spec/core/valid/instructions.html#variable-instructions) tracks operand effects and initialization of local indices; [Core 3 variable execution](https://webassembly.github.io/spec/core/exec/instructions.html#variable-instructions) changes the actual current frame on local assignment. These are separate states. A conservative validation merge may cease to prove a local initialized even when the path which really executed assigned it. This producer retains the validation proof in the immutable plan and records actual executed initialization in a private native byte array. It does not strengthen the guest validator's legality decision.

[Core 3 reference execution](https://webassembly.github.io/spec/core/exec/instructions.html#reference-instructions) distinguishes references and their heap types. The pre-op logical type comes from `runtime_operand_stack_storage_t.core_type` or its actual legacy declaration, not from the physical LLVM carrier. Validation-only unknown/Bot without a real live value declines that site. A physical tagged reference cannot establish an object's actual GC/external ownership or module subtype identity. Typed DATA commits check the conservative carrier envelope; later full export still requires actual canonical graph/root/subtype checks.

[LLVM alloca](https://llvm.org/docs/LangRef.html#alloca-instruction) allocates native frame storage with function lifetime. The actual retained LLVM 23 `llvm/IR/Instructions.h` supplies `AllocaInst::setAllocatedType`; this proposal changes the type only while the same function is still being emitted, before verifier/optimization/native publication. It does not resize a live native stack allocation or jump to any serialized PC. Existing opaque-pointer GEPs describe checked prefixes of the larger allocation. The unchanged Wasm typed ABI and immediate musttail/ret obligations are preserved; flag resets execute before the self-tail branch, never between a genuine musttail and its return.

## Exact production seams

`single_func_validation_dispatch.h` stages a tentative site before the current opcode consumes immediates or conceptual operands. It copies exact original-index local declarations/initialization proof and the conceptual pre-op operand deque. The same iteration commits its metadata after successful authoritative validation and original lowering. A lexical RAII transaction removes only its own last tentative cell on failure. No second body scan, whole-function prevalidation, guest cursor change or physical-type reconstruction is added. The terminating `end` is not staged because its original path completes the function before the common iteration tail.

The first producer supports reachable current function-context points with no active nested control or exception handlers and no saved control-entry parameters. Unsupported control/EH/Bot/resource sites leave `checkpoint_current_site` zero; they do not set the plan's `compiler_failure`, disable otherwise supported guest emission, reject valid Wasm or claim that a previous materialization is current. A later eligible point after a control merge can record its actual values while retaining the conservative validator proof. PHI/control/EH resumption itself remains unqualified.

`single_func_emit.h` owns four initially empty checkpoint-only fields. A selected immutable profile creates executed-local flags after actual parameter/default value stores. Parameters/defaultable locals begin true; nondefaultable declarations begin false. A reachable actual local.set/tee first stores the value, then stores flag 1. Actual self-tail parameter/default reset resets these flags before its branch. Control merges do not roll the executed flags back.

Before the existing actual debug safepoint bridge, the current site packs locals and live operand SSA values into N*16 native bytes plus exactly N-local flags. The LOAD of an uninitialized nondefaultable local exists only on the flag==1 edge. Unset slots are zero padding with their original nonnull type, not fabricated null values. Every original index, array extent, current-function LLVM owner, exact physical ABI carrier and bounded byte offset is checked before GEP/load/store.

One actual function-entry payload alloca grows to the maximum live bounded byte count seen in this same fused walk. One fixed saved-flags alloca is reused. Synchronous runtime copy finishes before the next point reuses the workspace. Native payload frame storage grows with maximum live slots, not opcode count times live slots. The old four-argument helper component interface reuses the actual emitter's fixed workspace owner cells when its workspace arguments are omitted; aliasing workspace owner cells or payload/flags owners rejects before packet IR. Full stack-budget qualification still requires actual optimized assembly and call-depth testing; this source does not claim a stack-limit or final performance pass.

`uwvm_runtime_generated_wasm_bridge.h` declares the real six-argument current-opcode bridge. The runtime implementation authenticates actual generated entry depth, canonical TLS activation, actual generation, full publication/source owner, same-control-block profile and independently sealed function plan before forming either native span. It checks slot multiplication, exact payload/local-flag counts, pointer-null and integer-end overflow before span construction. The native addresses originate only from the known generated frame allocation and do not escape. Integer bounds or matching public tuples alone never grant native-memory permission.

The four checkpoint materialize/activation bridges are also explicitly classified as internal control leaves, preventing a later generic selector from wrapping their genuine frame in a foreign-host island. The current producer already uses the unwrapped actual bridge symbol.

All local flags are checked for canonical 0/1 and required initialized states before any value byte is interpreted. The bounded packet factory makes independently owned DATA; the ledger then checks the same sealed-plan raw/control-block identity, exact slot types, initialized state and carrier envelope before a detached commit. Rejection retains prior frame DATA and recording failure is sticky. No collector/reference object token is dereferenced here. Unrooted aggregate, function, external and exception carriers cannot later authorize a durable checkpoint merely because the DATA copy succeeded.

The header runtime includes `dynamic_native_packet.h` before entering its namespace and the module runtime imports its exact exported module. This avoids relying on accidental transitive definitions or declaring a class in one named module then defining it in a different named module.

## Policy and ordinary-mode cost

Compilation cache identity policy revision is 5, with wire schema 3 and logical continuation version 1 unchanged. Each actual full publication and function plan retain the same immutable profile owner; a mutable global boolean cannot select compatible objects. The separate effects proposal's cache revision 3 must be composed into final revision 5, not applied afterward as a downgrade. This packet proposal alone does not qualify import replay/effect completeness.

Installing a nonnull profile before publication selects instruction-granularity debug points for that engine. It does not change the user's unwind versus instruction call-stack strategy. The existing genuine cold configuration/quiescence and host-admission gate remain necessary; this setter is not a world-stop issuer. An ordinary engine has a null plan, allocates no checkpoint flags/workspace, emits no added runtime packet/TLS probe or memory-access guard, and retains its existing observational debug granularity. These are source properties pending actual default-IR/optimized-assembly comparison.

The runtime proposal contains only a top-level header inclusion, a selected-profile granularity line and one runtime helper inclusion after the existing checkpoint helper. They must be rebased over the newly applied host-admission/cohort source receipt, not overlaid from an older full runtime snapshot. The compiler's DI provenance, full publication/native loaded-map ownership and GC declaration-policy regions are unchanged.

## Meaningful test source and execution plan

`debug_checkpoint_straightline_ledger.cc` is a cheap typed-DATA component. It checks stronger actual initialization with original static proof, exact plan/type/slot identity, noncanonical flags, abstract-heap mismatch, same-address/different-control-block rejection, sticky failure and preservation of earlier DATA. It grants no VM capture authority.

`llvm_jit_checkpoint_reusable_workspace_ir.cc` uses the real LLVM verifier and native MCJIT. It grows the actual function's entry payload owner from 7 to9 slots, emits 32 smaller later captures and a final full capture without another payload/flags alloca, verifies the module, then executes both assignment paths and a noncanonical-flag path. It checks exact i31, nullable/unset slots, NaN payloads, v128, operand and saved-parameter native bits. This is a workspace/payload component, not a proof that arbitrary control continuations can resume.

`debug_checkpoint_straightline_runtime.cc` takes `fixture.wasm instruction|unwind straightline|unsupported|overquota`. The straightline WAT uses a nondefaultable `(ref i31)` local, ref.i31, actual assignment, an assigned i64 local and an i64 operand at nop. Two real private pause episodes check canonical current-frame plan/generation, entry site1 with unset nondefaultable local, then site7 with both locals assigned plus one live operand; normal execution returns42. The existing borrowed display packet independently checks original-index i64 local bits. The scalar recording observation does not expose a complete payload/root authority. The nested-control WAT is legal and returns42 while its real interior stop explicitly reports no current materialized site. No valid module is rejected solely for an unsupported checkpoint point.

Required remote sequence, solely inside the existing owned64GiB cgroup lane: cheap DATA unit, real LLVM workspace component with actual verifier/native output and optimized stack assembly, then fresh runtime/main/host TUs from one immutable current source/provider closure in both repos, official wasm-tools parsing of all three WAT fixtures, six real fixture invocations per product (two stack policies by three scopes), and ordinary-mode IR/assembly comparison. Preserve all actual failures and before/after source/provider hashes. No local native testing, no old runtime object combined with this new bridge ABI, and no mock compilation or result is acceptance evidence.

## Remaining qualification boundaries

All-instance manager capture still requires one actual coherent guest cohort, nonwaiting actual host-operation closure, every canonical owner/source/compiled-plan generation, precise complete live frames and typed roots, all memories/tables/globals/segments/GC/EH/external state, guest FD/preopen/plugin/source-backing census, and a protected immutable management database asset capability. A source owner or mmap/fstat/checksum is not immutable file backing; external truncation and already-open guest aliases must be excluded by a real retained capability. Resource census remains unavailable.

Normal/tail calls, returned-child evidence, active EH, pending exceptions, original traces, GC/extern identity/cycles and rooted history, all thread continuations, host effects and deterministic schedule/replay adapters are not completed by this slice. Unknown foreign imports require the separately authenticated effects/admission bridge and explicit non-replayable rejection before any restore publication. Detached canonical endian codec PASS, a private closed gate or a DATA packet is not whole-VM rollback, save, restore or replay acceptance.


R2 allocation and alias preflight correction
-------------------------------------------

The R1 immutable source package remains a rejected predecessor. Its packet
workspace preflight used `isStaticAlloca()` as an extent check, but LLVM allows
a constant allocation count of zero in the entry block. R2 additionally
requires `!isArrayAllocation()` for every supplied local, executed-flags and
workspace allocation before its element type can grant any writable extent.
Assignment and self-tail reset helpers enforce the same single-object entry
contract. These checks happen while compiling the explicit checkpoint profile;
they add no ordinary-engine generated instructions.

Both reusable output owners must differ from the actual executed-flags owner.
Otherwise enlarging or zeroing the output could overwrite the initialization
ledger before the compiler emits its flag loads. R2 rejects both aliases before
any LLVM instruction, allocation-type change, or owner-cell mutation. The new
component constructs four genuine static zero-count allocations, tests the
assignment/reset rejection as well as packet rejection, and tests both alias
directions. It counts all function instructions and retains original owners and
types across each rejection. The subsequent actual native unset/assigned runs
remain required to prove the original flag values survive.

The LLVM checks for operand handles establish same-function identity and ABI
types. They do not prove SSA dominance. The trusted fused emitter supplies its
actual operand-stack handles and the complete module verifier must succeed
before engine publication; no general arbitrary-IR-handle API is offered.

LLVM primary implementation describes the two allocation predicates separately:
[LLVM AllocaInst implementation](https://llvm.org/doxygen/Instructions_8cpp_source.html).
The retained LLVM23 source excerpt is pinned in `primary-source-api/`; native
execution and verifier qualification must use the new R3 candidate rather than
the rejected R1 archive. Whole VM resource capture/restore remains unavailable.


## R3 native-storage budget and mandatory selected verification

The heap metadata budget is not permission to add an equally large native frame.
This producer separately bounds four checkpoint-owned allocation extents:
legacy entry locals (`locals*16`), the maximum live dynamic packet (`slots*16`),
actual initialization flags and saved flags (`locals` bytes each). It reserves
64 additional bytes for padding between these four owners, explicitly limits
checkpoint-owner alignment to 16 bytes, and requires the conservative total
`locals*18 + slots*16 + 64 <= 32768`. The subtraction/division preflight checks
both products and the semantic locals-plus-operands addition before creating
any LLVM alloca/GEP/store or changing an existing owner. Reused larger payload
extents must independently fit this budget; matching a smaller current site is
insufficient. Larger alignment also refuses before any IR mutation.
The bound covers additional checkpoint storage, not ordinary guest locals,
unchanged bounded observational display storage, ABI call spills or the whole
native call stack. Optimized assembly and deeper-call qualification remain
required. No OS or overall VM stack-limit proof follows from this arithmetic.

Entry or later eligible sites which exceed the quota record
`producer_availability=quota_exceeded`; this is distinct from malformed
`compiler_failure`. Entry decline may seal an inspectable plan with no invented
site. A later decline retains already emitted bounded workspace extents but
adds no more packet or flags IR. The actual activation records the decline
before any native payload read. The guest continues through its original
stores, controls, function body and typed ABI. Failure remains sticky for that
recording, so a prior site's DATA cannot silently become a current checkpoint.
Actual publication/profile/plan ownership is still required to observe this
status; a caller-created quota tuple grants no authority.

A selected immutable recording profile forces LLVM verification regardless of
an ordinary disable-IR-verification option. The cold compile options force
per-function/final verification before every worker; the emitter also forces
its verification state whenever a checkpoint plan exists. The actual full-task
optimizer verifies before and after optimization, and the actual merged module
optimizer/object-emission paths use the retained selected profile to force
verification. ROS already verifies full compilation unconditionally. Ordinary
engines keep their existing verification option and add no generated probe or
runtime memory guard. This is required because same-function/type checks alone
do not prove SSA dominance. Native execution and actual before/after optimized
IR verification of the new source are pending.

`debug_checkpoint_native_workspace_budget.cc` checks locals 961 versus 962,
operand-only slots 2044 versus 2045, SIZE_MAX/product bounds, a larger heap
metadata budget, a sealed entry-declined plan and sticky empty DATA ledger.
The LLVM workspace component preserves the R2 zero-count/alias cases and adds
valid large logical-site, oversized retained-owner and excessive-alignment
no-IR refusal cases. Its temporary fault-injection allocations are retired
before compiling the actual assigned/unset native packet function.
`checkpoint_workspace_overquota.wat` declares 1000 defaultable i32 locals and
returns 42. Its genuine runtime stop must report quota_exceeded, zero recording
frames/site/typed slots and no current materialized opcode, while the unchanged
bounded display remains available and normal guest execution succeeds.
The ordinary-product runtime fixture deliberately sets its legacy verification
opt-out before selecting the recording profile; ROS has no such option.
All of these are new source requirements, not native or whole-VM restore PASS.
