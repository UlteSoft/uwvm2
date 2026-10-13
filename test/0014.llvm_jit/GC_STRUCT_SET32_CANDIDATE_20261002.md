# Independent scalar mutable struct.set32 candidate

Status: source implemented, default disabled. No new candidate compiler,
native, VM, cache-hit, assembly or performance result is available yet.
`UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32=1` is independent of the six numeric
experiments, the frozen r11 two-header collector and precise trace metadata.
Undefined, zero and other values use the original lowering. Both products
contain the same helper/emitter/test bytes; their cache environment files
preserve their separate product contracts.

The candidate consumes the already validated `struct.set` immediate in the
existing fused translator. It has no bytecode scan or standalone validation
pass. Eligible declarations are mutable i32, f32, i8 or i16 struct fields;
actual native pointer width and endianness must match the LLVM DataLayout.
Reference, i64, f64, v128 and unsupported declarations retain the generic
bridge. The native ABI is explicitly revision 1:

```cpp
uintptr_t uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(
    uintptr_t module, uintptr_t kind, uintptr_t opaque_payload,
    uintptr_t field_index, uintptr_t raw_bits) noexcept;
```

All five arguments and the status are native register-wide integers. LLVM
zero-extends kind/raw i32 bits (an identity on i386), and uses an intptr field
constant. The helper rejects kind/index/raw above UINT32_MAX before any
truncation or object lookup. This avoids relying on unproven per-target narrow
C integer signext/zeroext attributes; high-bit Wasm i32/f32 values remain raw
unsigned bits rather than sign-extended native operands.

The module is the original owned compiler host-object relocation, never a
guest address. Payload conversion does not authorize a pointer dereference.
The helper builds a complete zero-initialized `gc_object_value::i32(raw_bits)`
and calls the original `store->struct_set`. For f32, LLVM emits a bitcast to
i32, preserving negative zero and NaN payload/signaling bits. The existing
store packs/truncates i8/i16. Actual token membership, object kind, real owner,
canonical layout, field bound, mutability, foreign owner lease and mutable
object lock remain in that same store route and order. This private helper
relies on the emitter's validated numeric-field contract exactly as the old
untyped carrier helper relies on its validated instruction; it is not a
public host API for writing arbitrary reference or wide fields.

The existing sealed wrapper still snapshots roots and retires before op5.
With the existing managed-page macro defined, both native routes call the same
`managed_page_boundary(false)` before object lookup. This candidate adds no
GC poll, guest frame, precise root, object cache, direct token read, mutable
fast lane, allocator or collector change. Null, bounds, OOM/size-overflow and
remaining-error trap mappings match the original aggregate emitter (including
its bounds trap kind). Inputs retire only after the success path; setters have
no Wasm result. The new call transports no input/output-buffer address.

## Native cache contract

The generated bridge name binds the actual C++ function identity, the printed
LLVM FunctionType hash and `gc_struct_set32_registerwide_v1` discriminator. The common
`get_llvm_runtime_bridge_function_symbol_value` re-registers the actual helper
address with `DynamicLibrary::AddSymbol` while reconstructing LLVM IR, before
an MCJIT cached object can resolve its external symbol. RISC-V64 retains its
existing direct native-address path and persistent-cache restriction.

The new exact-1 branch adds `llvm-gc-struct-set32-abi=registerwide-v1` to the
runtime ABI fingerprint for LLVM and interpreter/LLVM tiered builds. Undefined
or disabled builds retain the original fingerprint bytes. Both default cache
context constructors copy this fingerprint into `ctx.uwvm_abi`, which
`format.h` serializes into authenticated object metadata/path inputs. Distinct
symbol naming alone would not be sufficient to qualify a persistent cache.
Keeper must still prove actual macro values agree in CLI and runtime TUs,
actual source/ELF/SDK closure, process-restart symbol rebinding and cache hits.

## Source-only receipts and cold gates

Before-images are in
`build/wasm3-evidence/source-only-struct-set32-before-20261002/`. Frozen GC heads
are recorded but not copied/modified. Source preparation removes only the new
exact-1 conditional blocks and their separating include blank; the resulting
emitter/environment bytes match those before-images. This is deliberately a
text comparison, not a C++ preprocessing, machine-code or performance claim.

Run source preparation with the actual frozen root and a fresh evidence
directory:

```text
python3 test/0014.llvm_jit/prepare_gc_struct_set32.py --source-root ACTUAL_ROOT --out FRESH_EVIDENCE_DIRECTORY
```

Preparation extracts the original fixed-buffer native bridge verbatim into
`native_gc_struct_set32_baseline.h`. The native fixture includes that header
and the exact new production bridge fragment, without LLVM library linkage.
There is no independently rewritten substitute helper.
Preparation also changes only the first scalar readback expectation in a
separate `gc_struct_set32_scalar_bad_expected.wat` control; its valid module
must trap on both builds/oracle. It never changes a store to manufacture that
negative control, and is not included in successful performance inputs.

| Gate | Actual source/input | Required observation |
|---|---|---|
| Native ABI equivalence | `test/0017.runtime/wasm3_gc_struct_set32_bridge.cc` | Original status and exact separate readback; i32, f32 raw NaNs/negative zero, packed truncation, actual subtype, immutable field, field bounds, above-u32 host ABI misuse, null, empty/wrong-kind token, wrong aggregate, missing store/module, foreign lease and real reclaimed token |
| Native O3 output | Same fixture dynamic noinline wrappers | Final object/assembly for `uwvm_test_gc_struct_set32_raw` and `_buffer`; actual helper/backend bodies, calls, spills, value construction and lock/check paths; no assumed carrier elimination |
| Compiler IR/object | `llvm_jit_gc_struct_set32_ir.cc`, actual LLVM SDK | Nine declarations, exact five uintptr_t/status call, no gc.input/output scratch slots for eligible fields, f32 bitcast, field constant, preserved stack prefix, generic wide/ref/immutable fallback, symbol/type/version distinction and real AddSymbol address; verified input/O3 IR and native object |
| ABI cache gate | `llvm_jit_gc_struct_set32_cache_key.cc` | Undefined, 0, 2 and 1 separate compilation with actual runtime macros; exact-1-only key; real two-process object cache test separately |
| Full VM | Five WAT fixtures listed below, official binary/validation and Wasmtime oracle | Same bytes under candidate off/on, product LLVM full/lazy/tiered where supported; exact diagnostics/traps and readback; no change to feature flags |
| ASan/UBSan, optional TSan | Actual native fixture and compiled dependency closure | Native owner/token and old synchronization behavior; no sanitizer replacement of whole-VM tests |

The native fixture changes the real field to another value between baseline
and candidate calls, preventing a no-op candidate from passing on the previous
baseline store. Four native writers then use the actual foreign-owner helper
and locked getter, with valid per-writer data ranges; all readers/writers join
before collection. When the existing managed-page macro is defined, a separate
synchronous observer checks both helpers invoke the same false-boundary. That
observer is disabled before the concurrent stress, and restored after join;
it does not manufacture a managed-page capability or admitted VM entry.
The fixture explicitly labels its single-thread component sweep as an
aggregate-only exclusive component operation. It does not prove a VM pause,
native exception roots or automatic collection. It must not be mistaken for
the separately admitted metadata graph/census test.

Fixtures:

* `gc_struct_set32_scalar.wat`: real subtype passed as base, exact i32/f32/packed
  readback and unchanged immutable sibling; `_start` traps on any mismatch.
* `gc_struct_set32_null.wat`: valid instruction, runtime null trap.
* `gc_struct_set32_immutable_invalid.wat`: official validation rejection.
* `gc_struct_set32_field_invalid.wat`: official validation rejection.
* `gc_struct_set32_fallback.wat`: i64/f64/reference setter/readback use generic
  paths and preserve signaling NaN representation without FP arithmetic.

No WAT compiler/oracle has run locally. Negative validation fixtures need a
real official encoder capable of emitting invalid modules, not an encoder
that rejects text first and falsely substitutes that rejection for validation.
Keeper's normal Core 3 official tools should retain actual outputs and return
codes. Stale/foreign references are native component controls, because Wasm
source must not forge native store/token identities to manufacture them.

## Resource and performance binding

Only the Linux keeper executes compilation/native/WAT/VM checks in its actual
64 GiB, swap-zero scope, serial with other compiler/VM/profiler/Windows tasks.
Cold native/LLVM work uses the allowed E-core recipe. Performance waits for
cold correctness and an actual source/compiler/runtime/ELF/profile receipt,
then uses the established P0 frequency-qualified process contracts. No local
compile or VM execution is needed for this source packet.

The first minimal performance comparison is the already source-bound
mutable-struct **mutation** family, one identical binary at 1024 roots:
default six switches zero, candidate 0 versus 1. Separately repeat six switches
one with candidate 0 versus 1; neither comparison may silently change precise
trace metadata or r11 collector/directory flags. Bind both compile TUs and
cache state. ABBA repetition, actual checksums/live roots, unprofiled guest/
wall/user/sys/frequency, whole-guest grouped hardware counts and VTune samples
remain separate measurement families. Whole-guest counts include startup/JIT;
none is collector-only latency. Source operation estimates predict setter
exposure, not measured helper call counts or a speedup. Inspect actual O3 host
helper and actual finalized JIT loop before choosing whether this candidate
should progress beyond its default-off build gate.
