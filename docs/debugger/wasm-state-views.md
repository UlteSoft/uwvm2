# Current Wasm state views

The state reader returns copied Wasm data from an authenticated cooperative
LLVM-JIT-full stop. It does not inspect the virtual machine, evaluate native
expressions, follow arbitrary pointers, read registers as Wasm operands, or
authorize checkpoint restore. Query-local object identifiers express reference
identity and aliases within that one result; another request authenticates its
current capture and resolves its selected Wasm value again.

The schema follows the Core3 runtime model: values include numeric/vector data,
scalar i31, null, functions, structs, arrays, exceptions, host references and
external wrappers. Exceptions retain their actual tag identity and typed
argument values. A nondefaultable uninitialized local remains unavailable.
See [Core3 values and store](https://webassembly.github.io/spec/core/exec/runtime.html#syntax-ref),
[exception instances](https://webassembly.github.io/spec/core/exec/runtime.html#syntax-exninst)
and [call frames](https://webassembly.github.io/spec/core/exec/runtime.html#syntax-frame).

## Commands

- `globals THREAD MODULE [FIRST COUNT]` and `info globals ...` select the exact
  module's ordered global indices, including resolved canonical imported aliases
  only when the real runtime borrow can prove their owners.
- `table THREAD MODULE TABLE [FIRST COUNT]` and `info table ...` use an unsigned
  64-bit Wasm element offset. They never use a host address.
- `operands THREAD [FRAME [FIRST COUNT]]` and `info operands ...` select a real
  logical frame and its live operand tuple at the before-opcode site.
- `locals wasm THREAD [FRAME [FIRST COUNT]]` preserves original local indices,
  exact declared heap/nullability, and actual availability.

Default `FIRST=0`, `COUNT=16`, and `FRAME=0`; pages contain 1–64 rows. Numeric
payloads are copied as canonical little-endian bits and formatted using FastIO.
Floating-point values show their exact bits, preserving NaNs. Reference payload
bytes must be zero in the public view. Only resolved logical object IDs,
function module/index identities, nullness and i31 bits cross this boundary.
The formatter checks at most 64 objects and 256 members before formatting and
caps each complete response at 32 KiB. Struct/array cycles are supported by ID
edges; an external-wrapper-only cycle is rejected. Defined heap declarations carry their exact type-module origin. It can differ
from the actual object store/module after resolving an imported global/table
alias; the runtime must obtain both from authenticated canonical metadata.
The opaque host payload has
no display/read/serialization authority.

The console authenticates an existing status request, then asks the runtime for
a copied view using the actual current capture roster. The runtime canonicalizes
each opaque capture by both pointer and control-block identity before reading
its fields. A current complete cohort, private pause episode, exact code/source
owner and generation, real host-operation closure, actual GC lease/root census,
and publication borrowing are all required in one lexical transaction. Decimal
thread/module/frame indices and matching PCs do not replace those checks.

## Source and execution qualification

This R3 proposal is private source, not a claim that the current product can
already execute these new commands. The common view, grammar, formatter,
controller producer/consumer wiring and thin runtime wrapper are implemented.
The new runtime reader explicitly consumes the owner-private
`runtime_checkpoint_gc_state_borrow::copy_selected` behind the coherent manager.
The manager/GC producer and the immutable `observe_values` compilation policy
are being composed by their owners; they must be included and tested together.
The runtime default TU must include the borrow and reader before the coherent
manager, and the thin API after the canonical opaque capture wrapper.

An observer profile reserves the same-walk exact typed packet before compilation.
It does not emit a resume clone or confer a resumable checkpoint policy. Default
nondebug LLVM full has no additional generated packet, callback, SHA or guard.
An unsupported legal control/EH site reports incomplete typed-site availability
while leaving guest semantics unchanged. GC/exn objects are available only when
the actual root/store producer can safely copy them under the real transaction.

`debug_wasm_state_view.cc` contains bounded grammar/format/identity/corruption
tests, including table64 indices, cyclic GC aliases, extern wrapping, i31,
uninitialized locals and exception tag/payload rendering. It tests DATA only.
`debug_wasm_state_views.wat` supplies genuine Core3 struct/packed/v128/array,
table64, alias/cycle, extern-conversion and exception state for the separate
actual runtime fixture. A fixture must obtain its values from true before-park
capture and the checked cold API; copying the expected WAT values is not a test.
Actual native, named-module, big-endian, full-engine and performance results are
pending the isolated Linux keeper runs. No snapshot/restore command is added.
