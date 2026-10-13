# Debug-full local availability packet revision 2

This native observation packet belongs to the live generated function frame.
It is not a whole-instance checkpoint, GC export, portable continuation, native
register reconstruction or restoration capability.

The existing five-argument `llvm_jit_debug_safe_point_abi_bridge` contract remains
module, function, opcode offset, packet address and captured count. Every target
ASM wrapper therefore retains its existing register/stack ABI. Native producers
and observers must nevertheless be rebuilt together: the packet/view data ABI
has changed, and the object-cache policy includes `local-availability-v2`.

For N captured locals (at most 256) the allocation is a single LLVM structure
with two byte-array fields: `N * 16` complete payload bytes followed immediately
by `N` availability bytes. Both fields have alignment one and field zero begins
at offset zero. The runtime derives the second field only after verifying the
exact source declaration count, count/product limits and integer pointer extent.
The actual generated-frame owner and current execution/source/code generation
supply the lifetime proof; no arbitrary requested address reaches this callback.
Each availability byte is exactly zero or one. The runtime checks all flags
before handing a synchronous immutable borrow to the real park observer.

The compiler borrows the current initialization stack of the same fused
validation/emission traversal. Parameter locals and defaultable declared locals
are initialized by definition. A nondefaultable declared reference local is
available only when the current validation context proves its initialization.
The compiler emits flag zero and payload clearing without emitting a load from
that local alloca when this proof is absent. Index, declaration type and capture
count remain unchanged. Structural loop/else/end emitters invoke the same live
compile-time query after their current initialization-stack transition; no body
rescan, source-DWARF guess or new ordinary-full execution guard is introduced.

Consumers default every copied slot to unavailable, retain original indices and
check a flag before reading any of its payload. False is displayed as explicitly
unavailable. It is never formatted as zero, a null reference or a reconstructed
nonnull object. Source DWARF direct locals, frame-relative locals and pieces use
the same fail-closed availability contract.

Initialization proofs are conservative at structured control boundaries. A
local physically assigned within a nested block may lose its validation proof
at `end`, even while a native store remains. The observation packet can safely
hide that value. A complete checkpoint must independently materialize the actual
executed local-initialization state and cannot claim this display packet is its
complete state or executable continuation.

Acceptance requires freshly compiling all product/runtime/consumer translation
units, real debug-full Core 3 stops before and after first assignment, exact
indices/types/unavailable output, and LLVM IR inspection proving the unavailable
site has no load of that local alloca. The fixture is
`test/0017.runtime/fixtures/debug_nondefaultable_local_availability.wat`.
Ordinary full IR must remain unchanged. An old binary or synthetic callback
alone does not qualify the actual native product ABI.
