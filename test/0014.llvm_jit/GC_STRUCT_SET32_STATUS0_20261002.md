# Scalar struct.set32 success-first lowering

Source candidate only; disabled unless the existing
`UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32` is exactly 1. No new native, VM,
assembly or performance result is claimed. The r5 evidence and its nine-case
fixture remain immutable.

The r5 actual optimized IR and x86-64 object disassembly expose four ordered
status predicates on successful scalar setters: bounds, null, allocation
failure and finally zero. The first four fields in that actual nine-case
compiler component take this path. The r5 packet is
`build/wasm3-evidence/gc-set32-v5-ir-cache-cold-20261002-r5/`; the original
archive SHA256 is
`52efabe22a94505a2dc446c4b4d1b9d2c9e09aa666f5b6edba45fddadf0364fe`.
This establishes emitted predicates, not their percentage of a VM workload.

The candidate uses one real `status == 0` branch. Success reaches the existing
operand retirement. Every nonzero status enters one cold block, which selects
the original trap classification and directly calls the original runtime
trap. The 2000:1 branch weights express a static success preference; they are
not captured profile data. Optimized IR and actual assembly must prove that
the backend keeps the successful machine path to one status guard.

| Original status | Preserved trap kind |
|---|---|
| 0, ok | Continue without a trap |
| 3, null reference | null_reference |
| 6, bounds | array_out_of_bounds |
| 8 or 9, size overflow / OOM | gc_allocation_failure |
| Any other nonzero value | runtime_invariant_failure |

The five-register-wide setter ABI, actual bridge symbol/type discriminator,
and `store->struct_set` are unchanged. This does not bypass membership,
canonical type, mutability, field bounds, source owner, generation, foreign
lease or mutable-object locking. It creates no allocator, object cache, GC
poll, root, new host ABI or validation pass. Wide/reference/immutable fields
retain the original generic lowering.

The error path must call `llvm_jit_runtime_trap` directly from the JIT frame.
An extra native classifier bridge would change the return-address seed. The
candidate preserves the original trap FunctionType, calling convention,
NoTail, compiler memory clobber and explicit Win64 frame/stack-register
operands. RV64 trap-address materialization is emitted inside the cold block.
Its symbol is resolved before emitting the side-effecting setter, so a
resolution failure cannot emit a setter followed by generic fallback.

The existing exact-1 cache key changes to
`llvm-gc-struct-set32-abi=registerwide-status0-cold-v2`. The actual setter C ABI
and `gc_struct_set32_registerwide_v1` symbol discriminator stay revision 1.
The key change also protects builds without an embedded source ID from
reusing old four-predicate code. Other cache bytes stay unchanged.

The independent `llvm_jit_gc_struct_set32_status0_ir.cc` copies all nine r5
cases, including all five generic fallbacks and target-aware RV64 address
checks. It additionally verifies the real input IR diamond, the direct trap
identity/signature/context, cold-only status users, all ten original status
ordinals plus unknown/high-bit statuses, and the optimized success edge.
It emits input IR, O3 IR and the actual native object. Interpreting the emitted
integer SSA is a classifier check, not execution of a replacement helper or
whole VM. The old IR/cache tests are retained for the r5 frozen recipe; the
new cache test is the current v2 fingerprint test.

The unchanged Core 3 fixtures exercise real child-as-parent references,
mutable i32/f32, raw signaling NaN and negative zero, packed truncation,
null trapping, immutable/field-index validation failures and wide/reference
fallback. Their contracts follow the [Core 3 validation rule for
struct.set](https://webassembly.github.io/spec/core/valid/instructions.html#valid-struct-set)
and [Core 3 execution rule](https://webassembly.github.io/spec/core/exec/instructions.html#exec-struct-set).
Keeper must encode once with the official tools and use identical bytes for
the oracle and both products. No local WAT tool, compiler or VM was run.

Run the source-only preparation to obtain exact pins, before-images,
macro-off text projection and a focused cold plan. This projection proves
only byte equivalence of inactive source selection; it is not compiler or
assembly evidence. Actual cold execution belongs solely to the Linux keeper
in the current 64 GiB/swap-zero scope, serial with other compilation/VM work.
Use the independent nine-case unit, four cache values undefined/0/2/1,
unchanged real native setter equivalence and the same five WAT fixtures.
An existing runtime artifact may be reused only with its actual transitive
source/layout/compile-prefix closure checked; do not inject a source-ID macro
into just one TU. Fresh complete CLI/mmap safety and the mutable mutation
family follow cold success.

Compare old revision 1 and this revision 2 with SET32=1 and identical remaining
macros, runtime ABI, allocator/collector profile, cache state and Wasm bytes.
Keep default-six-zero and six-experimental-one comparisons separate. This
change alone cannot explain or fix the historical 117 ns/step allocation
workload: its setter exposure must be established from the actual loop and
helper profile. Unprofiled guest/wall/user/sys, whole-guest pure hardware
counts and VTune samples stay separate; none is pure collector latency.
Temperature is recorded only, frequency/actual PMU runtime/P0 identity/SMT
activity remain qualification evidence. No industry ranking is implied.
