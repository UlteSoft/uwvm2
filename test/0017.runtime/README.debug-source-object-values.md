# Source object layouts and copied values

This source change extends embedded DWARF type indexing to C/C++ structures,
classes, nonvirtual bases, unions, fixed arrays, signed enums, references and
DWARF5 `data_bit_offset` bit-fields. Type records are interned by actual DIE
identity; self-referential pointers are legal leaves. Qualifiers and typedefs
retain their names while exposing the underlying constant layout.

The public `query_type_layout` API provides member sizes and offsets for a
`ptype /o` style view. `query_object_value` accepts only already copied object
bytes and returns owned nodes with scalar values, enum symbols, array indices
and explicit unavailable reasons. Array output defaults to 200 elements per
dimension. Depth, edge, string, object-byte and result budgets are independent.
No partial result is published after a malformed graph or allocation failure.

`query_named_variable` selects an active concrete variable at a Code-relative
Wasm position, including lexical shadowing. `resolve_frame_relative_offset`
recognizes `DW_OP_fbreg` with a captured Wasm local frame base and computes a
checked Wasm32/64 offset. Neither API grants permission to read memory.
Production must authenticate the actual source owner, generation, stopped
participant and capture ticket, then use the runtime memory API to copy bytes
while maintaining the execution lease. A pointer value never becomes a native
address, is never followed automatically, and never opens a host file.

Composite `DW_OP_piece` and `DW_OP_bit_piece` locations have a finite decoder
and an owned-copy assembler. Empty or unsupported pieces, missing captured
locals and missing memory copies preserve explicit unknown-bit masks. Numeric
source-local display accepts a composite only when every bit is available.
No composite atom is allowed to contain another composite. Memory fragments
must match the exact requested source-byte index and still require production
capture/stop/epoch authority before the copy is obtained.

Dynamic member/bound expressions, virtual bases, legacy endian-sensitive
`bit_offset`, non-contiguous arrays and column-major value layouts remain
explicitly unavailable. The change does not execute source expressions or
DWARF calls, and does not yet implement a general location-expression stack,
Rust variant discriminants, pretty-printer scripts or overloaded C++ calls.
This is a specific improvement toward a language debugger, not GDB/LLDB parity.

The design follows [DWARF5](https://dwarfstd.org/doc/DWARF5.pdf),
[Wasm DWARF conventions](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md),
[GDB type inspection](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Symbols.html),
[GDB bounded value printing](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Print-Settings.html),
[LLDB formatting](https://lldb.llvm.org/use/variable.html), and LLVM23's
[DWARF type parser](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.1/lldb/source/Plugins/SymbolFile/DWARF/DWARFASTParserClang.cpp).

`debug_source_dwarf_objects.cc` exercises finite layout, copied-byte and offset
semantics. `debug_source_dwarf_objects_index.cc` consumes real Clang/Rust
producer output from three new fixture sources. The remote-only
`run_debug_source_dwarf_objects.py` recipe builds actual DWARF5 Wasm, validates
with official wasm-tools, records exact tools/source/input hashes and keeps raw
commands and logs. The keeper must independently qualify the metadata test's
LLVM link closure. Source-only AST/hash checks do not count as native passes,
and these tests do not by themselves qualify production guest-memory authority.
`debug_source_dwarf_pieces.cc` separately exercises split numeric values,
bit-piece offsets, explicit holes and copied memory fragments. These finite
component fixtures do not prove that a specific real producer emits composite
locations; optimized producer evidence must be collected independently.
