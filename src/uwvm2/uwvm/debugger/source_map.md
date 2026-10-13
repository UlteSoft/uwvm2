# Bounded Wasm source line map

`source_map::parse(module, sections, output)` consumes the **payload** of the
embedded `.debug_line` custom section and optional `.debug_line_str` and
`.debug_str` payloads. The caller supplies the Code section **content** length.
`lookup(module, code_offset)` accepts an offset from the start of that Code
content, not a function-relative offset or native JIT address. This follows the
[WebAssembly DWARF tool convention](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md#code-addresses).
The caller must still prove that a lookup address is a decoded Wasm instruction
boundary and an emitted debugger safe point. This component does not execute a
source-level step.

The parser copies paths into its own storage, opens no source files or network
locations, and leaves an existing map unchanged on parse failure. Its default
budgets are 16 MiB per supplied section, 1 Mi rows, 65,536 paths, 4,096 units,
4,096 bytes per path, and 8 MiB total path bytes. Caller-selected budgets have
hard ceilings. It rejects truncated LEBs and units, address overflow, offsets
past Code content, unterminated line sequences, invalid file/directory indexes,
and overlapping line ranges. A terminal `end_sequence` address is exclusive.
The sole out-of-Code address exception is an all-ones `DW_LNE_set_address`
tombstone from a linker-discarded function. The parser consumes that complete
line sequence without publishing any of its rows; other out-of-range addresses
remain errors. [LLVM lld writes -1 for discarded `.debug_*` function
relocations](https://github.com/llvm/llvm-project/blob/be955e5ac9a0b0c979221efb28b0f52aad7bd3d6/lld/wasm/InputChunks.cpp#L578-L595).

Supported line tables are DWARF 4 and 5, with 32- or 64-bit unit lengths,
4- or 8-byte Wasm addresses, standard line-state opcodes, dynamic file entries,
inline strings, `.debug_line_str` offsets, and `.debug_str` offsets. File indexes
are 1-based in DWARF 4 and 0-based in DWARF 5; the DWARF 5 state machine still
initializes the file register to 1, as in the [DWARF 5 specification](https://dwarfstd.org/doc/DWARF5.pdf)
and [LLVM's line-table reader](https://llvm.org/doxygen/DWARFDebugLine_8cpp_source.html).
Unknown line operations and forms requiring CU context, such as `DW_FORM_strx`,
fail with `unsupported_dwarf`. Split or external debug files, compilation-unit
directory resolution from `.debug_info`, inline call chains, variable locations,
and native PC mapping are outside this line-map component.

`test/0017.runtime/debug_source_map.cc` covers exact DWARF 4/5/64 intervals,
path and offset bounds, module isolation, failure atomicity and malformed input.
The Linux cgroup test additionally parsed linked Clang 20 C/C++ `-g` Wasm
artifacts for DWARF 4 and 5 in both repositories. A Rust 1.93 linked `-g`
Wasm fixture was built on macOS with a 4 GiB RSS monitor, then parsed under
ASan/UBSan in the Linux 64 GiB cgroup. Its many dead-function tombstone
sequences were discarded, leaving 40 live source ranges. These parser tests
do not establish source-step product behavior.

## Local Source Map v3 fallback

Without embedded .debug_line, the debugger may read the adjacent regular sidecar named by sourceMappingURL. fast_io open/status/read/close bounds the read to 16 MiB and checks file identity, size and timestamps before and after reading. Implicit loading accepts a local basename and rejects symlinks, FIFO files, network URLs, traversal/encoded paths, indexed maps and generated multiline maps. Failed metadata loading leaves Wasm controls available.

Generated columns are byte offsets in the entire Wasm binary. Checked loader Code bounds convert them to Code-relative offsets. The bounded parser checks JSON, UTF-8/Unicode escapes, signed base64 VLQ deltas, source/name indexes, unmapped gaps and duplicate points before publishing an owned line map. Display uses one-based source lines and columns. Source paths retain the declared sourceRoot; virtual paths need not identify local source text files. Embedded DWARF remains the first choice.

This fallback supplies source display and file/line breakpoints at real emitted guest safe points. Line-only source into/over/out uses each fresh joint runtime source/activation query and the original mapped coordinates. Over/out follows the actual guest continuation; source-map gaps are skipped without invented locations. A map supplies no variable/type location, concrete inline DIE identity, caller-frame selection or guest-memory read lease. Typed queries remain unavailable. Function replacement uses the existing source invalidation path. Host VM code remains outside ASM debugging scope.
