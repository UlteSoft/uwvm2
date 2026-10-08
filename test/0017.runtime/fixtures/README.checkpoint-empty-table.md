# Checkpoint state5 empty-table regression inputs

`checkpoint_empty_nonnullable_tables.wat` and its54-byte `.wasm` sibling cover
Core3 explicit table initialization, nonnullable typed function references and
both32-bit and64-bit empty tables. The same exact bytes are embedded in
`debug_checkpoint_codec.cc`; the declarative element segment is already dropped.
They follow the [official text table grammar](https://webassembly.github.io/spec/core/text/modules.html#text-table),
[table validity](https://webassembly.github.io/spec/core/valid/modules.html#valid-table),
[binary table declaration](https://webassembly.github.io/spec/core/binary/modules.html#binary-table)
and [binary table type](https://webassembly.github.io/spec/core/binary/types.html#binary-tabletype).

The C++ codec fixture tests canonical empty-table roundtrip, initialized-null
fabrication, nonzero length, real element placeholders, nullable canonicality,
nonzero carriers/reference kind, missing defined-type module, unused words,
retained roots, checksum-repaired wire corruption and stale state3 rejection.
Existing nondefaultable frame-local tests remain, along with operand/global/GC
field placeholder negatives. The complete cache tuple fixture isolates schema3
and schema5; binding/envelope fixtures reject stale state schema/profile DATA.

This private packet has not compiled or run C++/Wasm. Official reference assembler
byte-equivalence, actual fused validator full/lazy modes, Linux native little-endian
and actual QEMU big-endian C++ codec runs remain required. No host credentials,
whole-resource census producer, persistence publication or executable restore
authority are granted by these tests or detached state DATA.
