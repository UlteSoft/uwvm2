# Native typed ABI checkpoint context, independent source candidate 1

`single_func_checkpoint_context_emit.h` provides an independent real LLVM
prologue mechanism. It is not selected by the shared emitter or runtime. The
actual generated Wasm function keeps its original `FunctionType`, parameters,
return type and calling convention. A six-integer component test ABI is never
substituted for a Wasm typed entry or its indirect/ref/tail-call ABI.

Only a nonnull immutable per-engine checkpoint plan emits the context. A null
plan returns before metadata validation, allocation or IR construction. Normal
LLVM-JIT-full, lazy, tiered and interpreter code gains no context call, TLS
probe or memory access. Production selection requires a new cache policy
identity jointly covering the actual bridge and continuation schema.

The compiler creates a function-owned native LLVM aggregate with five fields:
`{i64 logical_site, ptr payload, uintptr payload_bytes, ptr original_flags,
uintptr flag_count}`. Its target DataLayout owns padding and offsets. A selected
helper first rejects unspecified DataLayout and any pointer width incompatible
with this actual runtime native ABI, before emitting allocation, call or GEP.
The integer field type is taken from that validated DataLayout. A cross-target
JIT cannot silently reuse a host-width context. This is
private native state, never the canonical little-endian database schema. The
future actual bridge must derive the identical layout from its target ABI; it
cannot reinterpret portable bytes or guess a C++ `sizeof`/`offsetof` layout.
Each field GEP/read follows complete type and owner checks. The input packet
is not read here: the existing logical landing verifies the full typed extent,
nonnull addresses and all original-index flags before any payload load/local
mutation, with separate real immutable ownership required of its dispatcher.

The exact context bridge ABI is `void(i64 module, i64 function, ptr out)` in
the actual module with C calling convention and genuine `nounwind` behavior.
Varargs, other signatures, foreign owners, intrinsics, `noreturn` and return or
parameter ABI modifiers are rejected before IR. The helper never infers bridge
trust from its name, a public pointer, attributes or metadata. Those checks are
compiler consistency only; real bridge issuance remains a private runtime
obligation. The context starts at zero. A genuine bridge may leave it zero only
when no resume work exists. If actual resume work is pending but its canonical
frame/owner/generation checks fail, that bridge must report/reject the management
failure rather than silently taking the normal path.

## Real runtime producer still required

An authenticated private resume mailbox must be installed only after the
whole-instance restore transaction has prepared new typed frames, module/type/
function owners, materialized packets and a new real generation. The bridge
must consume exactly the currently entering activation's mailbox once, and
match actual runtime epoch, module/function generation, retained sealed plan,
source/publication owner and immutable profile by pointer **and control block**.
The generated-entry capability and current runtime generation lease must both
be genuine. Caller-supplied module numbers, a thread-local boolean, copied
logical DATA or an equality of native addresses cannot mint that permission.

The new dispatcher cannot reuse old parked native frames against a replaced
instance. Those frames need a management exit/cleanup protocol that retires
scoped roots and leases before the new logical continuation is rebuilt. It
cannot call `stop_and_drain()` while it waits for those live parked leases, or
jump to serialized native PC/SP/GPR values. All-thread root/host resource census,
failure-atomic instance generation commit, code provenance invalidation and
host-effect adapters remain separate mandatory work. Unknown host imports are
non-replayable; a zero host-operation count is not external-world rollback.

The helper call belongs before logical dispatch and actual opcode execution.
It does not modify `musttail` signatures or insert anything between a true
`musttail` and its immediate return. On normal execution of an explicitly
checkpoint-instrumented engine this one selected context call is deliberate;
ordinary engines emit none. EH/GC/extern and call-result continuation completeness
is not inferred from the existence of this prologue.

## Native component candidate

`llvm_jit_checkpoint_typed_abi_context_ir.cc` emits real functions of native
typed ABI `i64(double, i64)`, an actual same-signature `musttail` wrapper and a
driver-owned LLVM context supplier. It verifies the complete module and invokes
the entries through a live MCJIT engine. The supplier uses real engine-owned
globals and compiler-owned output memory; it is explicitly a DATA test fixture,
not a private VM resume issuer. A floating parameter proves that restore fields
are not smuggled into or substituted for typed guest parameters.

The component selects the real native target and installs its DataLayout before
emitting IR. Missing layout and deliberately foreign pointer width both reject
before IR. It checks normal result 16, same-ABI tail result 19, logical entry
result 83, and operand-site tail restore result 154. Wrong bridge signature,
foreign module, unproved unwind behavior and hidden parameter ABI attributes
fail before any IR. Unknown sites, wrong packet extent, invalid flags, an unset
required parameter and null payload return the reject sentinel before input
mutation. A separately emitted ordinary typed function still returns 24 while
the context is invalid and without incrementing the supplier counter. An
immediate `musttail`/return pair and entire-module verifier are checked.

Run only on the guarded remote lane against the current matching LLVM/provider
source closure, preserving IR and optimized normal/restore/tail assembly.
Native component execution is pending. This source is not accepted whole-VM
save/restore, reverse execution or deterministic replay.

The ABI and tail constraints follow the primary
[LLVM call instruction reference](https://llvm.org/docs/LangRef.html#call-instruction)
and [calling conventions](https://llvm.org/docs/LangRef.html#calling-conventions).
The logical opcode/call continuation follows the
[Core 3 execution model](https://webassembly.github.io/spec/core/exec/runtime.html).
