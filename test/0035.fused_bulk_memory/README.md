# Same-pass bulk memory candidate

This PRIVATE source candidate has not been applied, compiled, or run. Its
parent is the separately frozen atomic R2 candidate, rebased on the actual
page/SIMD, Stage4 declaration and common typed-stack kernel source changes.
Root must compose the exact descendants before giving the sole Linux keeper a
fresh source/product closure. These source checks cannot authorize execution or
qualify the current working-tree products.

The pure Core3 validator, integer translator and LLVM translator consume the
first bounded decoder's checked data/memory indices and selected address types.
One shared typed sequence implements all three operand rules: memory.init uses
the selected destination address type and two i32 operands; memory.fill uses
the destination address type for address and length, with an i32 fill value;
memory.copy uses the source and destination address types and their minimum
width for length. Only an i64/i64 copy therefore consumes an i64 length. The
common kernel checks total reachable arity before popping any operand. Value
Bot and reference-only heap Bot remain distinct. Data.drop consumes no operand.

The resulting owned LLVM event is copied from that successful transition. The
normalized emitter reads no expression slice and does not decode FC8--FC11
immediates again. Those four raw generic cases are removed. A successful typed
frontend marks emission inline before calling this emitter, and an emission
failure disables that path rather than replaying its bytes. Original LLVM
bulk-memory bridge/lowering helpers and integer register-ring emission after
the typed transition remain byte-for-byte unchanged. The checked event is
compiler data, not a runtime memory capability or root authorization.

Rules follow the official [Core3 validation instructions](https://webassembly.github.io/spec/core/valid/instructions.html#valid-memory-copy)
and [execution instructions](https://webassembly.github.io/spec/core/exec/instructions.html#exec-memory-copy).
Existing runtime bounds and trap behavior still require actual validation.
Preserving helpers also preserves their pre-existing local host-provider
memory64 fill/init emission limitation; a separate provider bridge fix and
real component fixture are pending. Plain defined and Wasm-imported native
memory are the test scope here. This candidate makes no claim that the
host-provider limitation or all remaining Core3 lowering is complete.

The paired test/0035.fused_bulk_memory leaves contain 20 return assertions,
14 runtime trap assertions, ten invalid WAST modules, five malformed binary
encodings, an unused invalid function at index31, and a constexpr nine-event
typed-sequence component. They cover both address widths, all four copy pairs,
forward/backward overlap, fill truncation to a byte, data initialization,
idempotent data.drop, and zero-length endpoints and out-of-range positions.
Each memory has one initial and maximum page. Trap modules run separately;
they do not prove unchanged memory after a trap. A state-after-trap witness is
explicitly pending rather than inferred from a failed subprocess.

The finite actual runner pins tools, products and its shared wrapper helpers,
uses the existing cgroup admission check, and requires real return, rejection
and runtime memory-trap results. Full mode selects interpreter/JIT full and
the validator. All mode includes lazy/tiered configurations to expose their
remaining admission gaps. Root and the sole keeper must execute within the
existing 64GiB sandbox. C++ compilation, WAST assembly, fresh products, runtime
oracles, LLVM IR/assembly and performance are pending. Retained lazy plans,
tiered reuse and complete validation before observable initialization effects
are not completed by this slice.
