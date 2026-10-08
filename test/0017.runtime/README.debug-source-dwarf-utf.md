The language scalar decoder previously reported every `DW_ATE_UTF` base type
as unavailable. Clang emits this encoding for C++20 `char8_t`, `char16_t`, and
`char32_t`; Rust emits it for `char`. This change exposes exact unsigned code
unit bits for byte widths 1, 2, and 4 through the existing copied-local and
owned guest-object query paths. No string decoding or Unicode normalization is
performed. A UTF-8 continuation unit and an individual UTF-16 surrogate can be
inspected without guessing adjacent bytes. Unknown encodings, pointers in the
numeric-local path, float/reference carriers, and 8-byte UTF remain unavailable.

Primary references:

- DWARF 5, section 5.1 and Table 7.11: `DW_ATE_UTF` is `0x10`.
  https://dwarfstd.org/doc/DWARF5.pdf
- Accepted DWARF issue 090109.1 includes `char16_t` byte-size 2 and `char32_t`
  byte-size 4 examples. https://dwarfstd.org/issues/090109.1.html
- Clang `CGDebugInfo::CreateType(BuiltinType const*)` maps Char8/16/32 to
  `DW_ATE_UTF` and uses the AST target type width.
  https://clang.llvm.org/doxygen/CGDebugInfo_8cpp_source.html
- Rust `build_basic_type_di_node` maps `ty::Char` to `DW_ATE_UTF` and uses the
  target layout size. https://github.com/rust-lang/rust/blob/master/compiler/rustc_codegen_llvm/src/debuginfo/metadata.rs
- Rust guarantees `char` has `u32` size, alignment and function ABI; it is always
  four bytes. https://doc.rust-lang.org/std/primitive.char.html#representation

`debug_source_dwarf_utf_values.cc` is a finite component test of numeric-carrier
widths, source-width masking, implicit values, unavailable slots, copied guest
aggregate/array values, selectors, incomplete known-bit masks and bounds. Whole
native i32/i64 carriers are constructed in native representation; guest bytes
are explicit Wasm little endian, making the same source meaningful on little-
and big-endian targets. It never acquires a runtime read capability.

`debug_source_dwarf_utf_index.cc` requires actual linked Wasm fixtures produced
from `fixtures/debug_source_utf_cpp.cc` and `fixtures/debug_source_utf_rust.rs`.
It parses production LLVM DWARF, requires the exact UTF encoding and target
widths/layout, then uses those genuine metadata records to query owned bytes.
A synthetic type record cannot satisfy this producer check. Compile with full
DWARF 5 at O0, validate the final Wasm using official wasm-tools, then run the
index component with the `cpp` or `rust` label. Normal execution of both fixture
`_start` exports must finish without traps; the expected checksums are 0x3b103
(C++) and 0x12fa03 (Rust).

All native compilation/execution remains pending Linux keeper admission in the
existing 64 GiB cgroup. No native build/run, cross-platform result, performance
result, runtime stop authority, or GDB/LLDB parity is claimed by this source
proposal. No WASIp1 implementation, source safe point, LLVM emitter, guest-memory
read path, or frame-base expression-role handling is changed.
