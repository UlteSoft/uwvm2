# Logical LLVM continuation landing, independent compiler candidate 2

`single_func_checkpoint_resume_landing_emit.h` emits real LLVM basic blocks,
selector switches, typed local stores and SSA PHIs. It is not included/selected
by shared compilation or runtime yet. `llvm_jit_checkpoint_resume_landing_ir.cc`
is a real LLVM verifyModule/MCJIT native component candidate, not a fake save or
restore result produced from a portable codec. Native execution is pending.

## Actual IR shape and contracts

The helper is selected only by the actual emitter's nonnull immutable checkpoint
plan/profile. Default nullptr returns without blocks, calls, loads, stores or
policy guards. Its result is explicitly `selected=false`, so ordinary callers
retain original SSA handles without copying/allocating a replacement vector.
Only a selected result supplies replacement PHIs. It accepts same-function
LLVM handles and checks the complete
actual fused metadata identity before constructing site IR. Physical type
equality checks native ABI only: independently supplied exact Core 3 nonlocal
declarations must also match, including heap and nullability. Copied site DATA
or a foreign function's native handles do not replace the real builder site.

A generated function's optional prologue switches on a logical dense site
ordinal; zero follows normal execution. Only explicitly installed, validated
`before_opcode` sites become cases. An unknown ordinal enters the compiler's
explicit reject block. The future real native dispatcher must supply that
selector from the privately authenticated, immutable logical frame; a client
number does not become a native address, branch label or resume capability.

Each site first checks the expected complete native packet and original-index
flag extents plus required nonnull input addresses. Every local flag is loaded
and validated before ANY value payload read or native local/flag mutation.
Flags must be 0/1; a statically proven-readable local must be one. Only after
the complete check succeeds are actual executed flags and local values restored.
A nondefaultable local's payload load/store sits on the real `marker == 1`
branch. Its zero edge does not load the unavailable value. All typed payload
loads use bounded 16-byte slot offsets with alignment one. Complete constructed
native carrier/type/store validation and actual immutable pointer ownership
must have succeeded in the private runtime manager before handing off to this
IR; integer extents alone grant no memory permission.

At the logical site, the normal edge and restore edge merge through actual SSA
PHIs for every live operand followed by every saved control-entry parameter.
The fused compiler must replace all corresponding live handles with those
returned PHIs, preserving exact logical declarations and declared control
tuples separately. Native local allocas remain the same function's checked
owners. A full-function verifier/dominance check remains mandatory after all
sites and control edges have been emitted. The helper does not reconstruct
semantic types from LLVM SSA values or skip the single validate/translate walk.

## Deliberately incomplete executable obligations

This is a real logical LLVM landing mechanism for individual before-opcode
sites, not a whole-instance dispatcher. Active EH handlers and exception
continuations are rejected until real tag/payload/reference owners and cleanup
edges are materialized. Awaiting-child-return sites need a separate exact
caller-result continuation: the private dispatcher must execute the leaf,
transfer its exact results into the parent and resume after the call without
repeating it. Loop/if/else/end/saved-parameter and every PHI predecessor must be
bound by the fused compiler; the independent helper does not manufacture those
actual Wasm control edges from sample metadata.

Typed-tail/new-frame reset and actual return/EH cleanup must preserve the
private activation and continuation identities. No work may be inserted
between a genuine musttail and its required immediate return. Static validator
initialization legality remains independent of physically restored executed
flags. Restoring a stronger actual nondefaultable local cannot authorize an
illegal `local.get` under the function's semantic validation proof.

The whole runtime additionally needs the private all-thread coherent stop,
closed host admission, canonical code/type/store/root ownership, detached
complete instance graph restore and one failure-atomic generation commit.
Historical GC/native-exception/external references need genuine lifetime and
root/replay registrations before any collection can reclaim them. Unknown
host imports stay non-replayable. No native PC/SP/GPR or native stack blob is
used by this mechanism, and no save/restore/reverse/replay command is exposed.

## Meaningful native component checks

The new test constructs real LLVM functions against the project's actual emit
state and helpers, verifies the entire module, and resolves the emitted native
entry through the live MCJIT engine. Its actual integer-only component ABI is
explicitly separate from a public Wasm function ABI or runtime authority.

The two logical cases restore exact locals (i64, unset/assigned nonnull i31,
complete v128), typed operands and saved control parameters including a
nonnull i31. Normal selector zero reads no restore input; logical entry one
continues with the normal subsequent values; logical site two takes the actual
restored SSA predecessor. Results and newly captured complete native bits
prove the correct execution edge. Wrong semantic nullability with the same
physical reference carrier and a copied metadata site reject before any IR.
Noncanonical flags, an unset proven numeric local, wrong extent/null input and
unknown selectors reject before output mutation. An unset nondefaultable local
keeps an unavailable zero slot without loading its packet/native alloca.

Run these new tests only on the sole remote cgroup lane with the exact current
LLVM/provider/fast_io headers and ABI/macro closure. Neither old runtime code
nor portable endian-codec acceptance qualifies this native component. Preserve
LLVM IR and optimized assembly for normal/restore/reject paths when reviewing
the helper. Actual native component and whole-VM qualification remain pending.

The logical representation follows [Core 3 configurations and continuations](https://webassembly.github.io/spec/core/exec/runtime.html)
and [function/local/control execution](https://webassembly.github.io/spec/core/exec/instructions.html).
LLVM [StackMaps](https://llvm.org/docs/StackMaps.html) and
[Statepoints](https://llvm.org/docs/Statepoints.html) supply compiler mechanisms;
their native register information alone is not the complete portable Wasm
instance, external-resource or resumed logical call/EH protocol.
