# Source variants and global metadata: v3 source qualification

The language metadata owner adds bounded Rust/Ada-style DWARF variant parts,
global/file/class/function static metadata, and already-copied object-bit display.
This is not a claim of GDB/LLDB parity, actual guest-memory bridge qualification,
native PASS, continuation recovery, or general expression execution.

The semantics follow [DWARF5](https://dwarfstd.org/doc/DWARF5.pdf),
[Wasm DWARF conventions](https://raw.githubusercontent.com/WebAssembly/tool-conventions/main/Dwarf.md),
[LLVM's discriminator emission](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.1/llvm/lib/CodeGen/AsmPrinter/DwarfUnit.cpp),
[Rust native enum metadata](https://github.com/rust-lang/rust/blob/main/compiler/rustc_codegen_llvm/src/debuginfo/metadata/enums/native.rs),
and [GDB Rust type/value behavior](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html).
LLVM permits a discriminant in the same containing aggregate as a variant part,
the [DWARF6 clarification](https://dwarfstd.org/issues/180123.1.html); an unrelated
CU/object discriminator does not become a field of this object.

`DW_TAG_variant_part` owns a finite discriminant member and variants. Supported
integer discriminant types are 1/2/4/8-byte scalar/enum types. `DW_AT_discr_value`
uses that type's signedness. `DW_AT_discr_list` parses `DW_DSC_label` and inclusive
`DW_DSC_range` with fast_io SLEB128 for signed types and ULEB128 otherwise. Width
overflow, inverted ranges and truncated descriptors fail with no partial output.
Defaults are selected only after all explicit selectors miss; duplicate defaults
or multiple matching selectors are ambiguous. A single untagged default variant
needs no invented discriminant. Unsupported niche representations, virtual or
dynamic offsets, complex expressions and nested variant selectors stay explicit
unavailable. Known direct members remain inspectable.

`query_object_value_with_known_bits` accepts owned/copied bytes and an equal-size
availability mask, never a memory-reading callback. It checks each scalar's full
range and each bit-field's exact used bits before display. Unknown discriminant
bits prevent variant selection; they do not erase fully known unrelated fields.
Type layout lists variant metadata, while value layout exposes the unique active
variant only. Metadata and copied bytes are not serialized execution state.

Global lookup requires a genuine source stop's physical scope before selecting
names. Lexical locals shadow globals. Current-CU definitions have priority;
file statics from unrelated CUs are unavailable, while explicitly external unique
definitions can be named across CUs. Declaration-only DIEs are never definitions.
Exact producer-qualified names support namespace/class/Rust paths. This does not
implement GDB's current-crate inference, arbitrary member expressions or calls.

`DW_OP_addr` and a terminal DWARF5 `DW_OP_addrx` identify only guest offsets under
the single-memory Wasm producer ABI. Address-table contribution header, version,
width, segment size, declared length, entry stride, exact `addr_base` and index
are checked before LLVM's indexed API. No host symbol/address lookup, TLS
evaluation, source-file loading or host-pointer dereference is performed. Runtime
integration still must authenticate the same participant ticket, frame and
source/code generations, prove exactly one native-defined unshared memory with
matching address width, then copy under its memory lifetime/extent protocol.

The Linux keeper alone runs `run_debug_source_dwarf_variants.py` in the existing
64 GiB/swap0 cgroup with independently pinned compiler/SDK/link inputs. Cheap
components cover signed ranges/default ambiguity, partial copied-bit precision,
global shadowing and offsets. Fresh LLVM-DWARF metadata executable input is
`debug_source_dwarf_variants_index.cc`; its actual link closure must be built and
verified by the keeper, not inherited from an earlier binary. Genuine Clang C/C++
DWARF4/5 and Rust DWARF5 fixtures are compiled to objects, linked, officially
validated, and then read through that executable. Rust signed payload enum and
`Option<NonZeroU32>` source intent cannot replace actual emitted metadata evidence.
Source, producer object, final Wasm, compiler tools and native executable hashes
are recorded separately before/after each frozen qualification.
