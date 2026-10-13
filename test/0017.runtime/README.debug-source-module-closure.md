# Language module dependency and formatting audit

This is a source-only audit. Neither this document nor the isolated capsule
qualifies the complete debugger, genuine producer DWARF, runtime memory-copy
authority, the LLVM local-availability packet, continuation recovery, or
GDB/LLDB parity. Keep the v1–v6 freezes immutable; changed language leaves and fresh
module seeds need their own native build evidence in the Linux test cgroup.

Both product debugger aggregates explicitly include/export all eight
`source_dwarf_*` header/partition pairs. Their direct import graph is acyclic:

| Partition | Direct debugger imports |
| --- | --- |
| source_dwarf_types | none |
| source_dwarf_variants | types |
| source_dwarf_pieces | types |
| source_dwarf_query | types, source_scope_path |
| source_dwarf_values | types, query, pieces |
| source_dwarf_objects | types, query, values, variants |
| source_dwarf_selectors | types, query, values, variants, objects |
| source_dwarf_index | types, variants |

`source_scope_path` directly imports `source_dwarf_types`; it is a shared
dependency owned by the scope/checkpoint work. Every language partition now
directly imports `fast_io`. The index global fragment explicitly includes each
standard header for the standard names its header uses, including array/span/
string_view, rather than relying on an incidental LLVM SDK include. The header
branch includes the same standard dependencies.

Language metadata/result strings remain owned `std::string` objects with their
existing bounds and budgets. Their explicit formatted copies use bounded
`std::string_view` operands and `fast_io::concat_std`. The vendor's old `concat`
stdstring entry is deprecated and is absent from the module export surface;
`concat_std` is the return-type-compatible front door. The fresh fast_io owner
snapshots also export `parse_result`, standard-string format hooks and
`basic_ostring_ref_std`. They must be applied to the actual candidate before
rebuilding the fast_io PCM. A previous PCM cannot qualify these new exports.

The header path also enters `fast_io_unit/string.h` explicitly after its
standard-string headers. The hosted umbrella selects this optional unit from
already-present standard-header macros; a parent that included the umbrella
earlier must not silently suppress the language layer's stdstring hooks.

The aggregate component fixture now emits Wasm little-endian bytes through
`fast_io::little_endian`, followed by a bounded copy of the owned scalar. It
checks vector growth before resize and derives the destination only after a
positive width and successful resize. Its all-five-character escaped-output
golden uses `basic_ostring_ref_std` plus `print(code_cvt(bounded_view))`.

For actual module qualification, the keeper should pin and record the complete
fresh compiler/SDK/standard-library/fast_io input closure, use a new output
directory, and disable implicit modules. Precompile the fresh fast_io module,
then the production partitions in this order: types, source_scope_path,
variants, pieces, query, values, objects, selectors, index. Supply explicit
`-fmodule-file=fast_io=<fresh PCM>` and explicit prior partition name-to-PCM
options at every dependent command. Precompile
`debug_source_dwarf_module_capsule.cppm` with those exact partition references;
compile and run the import-only `debug_source_dwarf_module.cc` using that
capsule's PCM and the fresh fast_io PCM. Compile/link matching PCM objects as
required by the compiler; do not reintroduce header includes into the consumer.

The capsule exports the actual production language partitions under their real
primary module name, but deliberately does not substitute for compiling the
production aggregate. Perform both the cold no-LLVM profile and the profile
with the actual product LLVM macros/includes/link closure. The latter consumer
also checks the exported LLVM-backed index owner type. Record source hashes,
dependency files, exact commands and output for both repositories. Header-only
component success cannot establish module export success.
