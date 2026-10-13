# Mixed table.copy declaration witnesses

LLVM's existing fused walker checked rich reference types only when both source
and destination declarations had explicit witnesses. An MVP `funcref` table
uses `has_core_type=false`; equal 0x70 carriers therefore admitted a forbidden
copy from erased `funcref` to a typed function/struct destination. INT and pure
wasm3 normalize each declaration separately and reject that conversion.

This narrow LLVM correction normalizes each missing witness with the existing
`core3_legacy_carrier_type` and invokes the existing context-aware matcher. It
adds no Wasm body pass, no cursor movement, no guest LLVM IR, and no access guard.
All legacy-to-legacy operations remain identical. The inverse typed-to-erased
copy is valid and executes a selfcheck through call_indirect.

The four WAT cases require official wasm-tools assembly and validation, then
actual pure wasm3 and fused INT full/lazy plus LLVM full/lazy. ROS uses both full
modes. Valid cases run independently under Wasmtime; invalid code must produce
the actual code-validation diagnostic. Generic fatal, lowering/artifact decline,
parser/CLI failure, crash and timeout never qualify. All invalid bodies are the
entry body, so the pending whole-module lazy admission patch is not needed for
these cases. For each instruction/unwind strategy: ordinary16 product cells,
ROS8 cells, plus pure/oracle rows. Use the identical command interface documented
by run_table_copy_rich.py --help inside the original Linux keeper cgroup; never
run the script locally on Mac. Supply actual binary/source/build receipts.

No cache revision change: full and demanded-lazy fused checking occurs before
object cache admission; formerly accepted invalid bodies now reject before
lookup. For every still-valid body, emitted IR, object ABI and execution semantics
are unchanged. This relies on the actual fused-before-cache path; it does not
claim unused-body lazy admission is already complete.

Primary sources: [Core 3 table.copy](https://webassembly.github.io/spec/core/valid/instructions.html#valid-table-copy),
[reference matching](https://webassembly.github.io/spec/core/valid/matching.html),
[official wasm-tools](https://github.com/bytecodealliance/wasm-tools/blob/main/README.md).
SOURCE-only initial state: no official parse, native compile, Wasm execution,
assembly inspection, performance test or complete-conformance claim.
