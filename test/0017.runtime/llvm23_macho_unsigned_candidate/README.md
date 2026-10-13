# LLVM 23 MachO loaded-DWARF unsigned relocation candidate

This directory is a source proposal. The vendored ROS LLVM files are **not
modified**, and the ordinary product's configured LLVM is not replaced. The
patch targets the two source identities in `source-identity.json`. Applying or
compiling it in an isolated SDK is part of qualification, not a recorded pass.

## Source contract

The checked ROS source currently returns `S` for x86_64 unsigned fixups and has
no AArch64 MachO resolver. Its DWARF local-relocation path initializes `S` with
the original section base before the loaded-section adjustment. Adding the
in-place field without first changing that local contract would count the
original base twice.

Let `O` be the original target-section address, `P` its actual loaded address,
`o` the label offset in that section, `A` the expression addend, and `w` the
actual relocation-field width. All arithmetic is unsigned modulo `2^w`.

| Relocation | In-place value | DWARF resolver `S` | Final value |
| --- | --- | --- | --- |
| Local section | `O + o + A` | `P - O` | `P + o + A` |
| External symbol | `A` | actual loaded symbol address | `S + A` |
| Local `R_ABS` | absolute in-place value | zero, no target section | in-place value |

The existing `LoadedObjectInfo` convention uses a zero section-load address to
mean that no adjustment is available. This proposal preserves that convention.
It also preserves DWARFContext's existing MachO behavior without LoadedObjectInfo:
relocations are skipped, and raw object coordinates remain raw coordinates.
Already-relocated section contents reported by LoadedObjectInfo are likewise
not relocated twice.

The external-symbol and local-section encodings follow Apple's format sources:
[x86_64 relocations](https://github.com/apple-oss-distributions/cctools/blob/main/include/mach-o/x86_64/reloc.h),
[AArch64 relocations](https://github.com/apple-oss-distributions/cctools/blob/main/include/mach-o/arm64/reloc.h),
and [common relocation fields](https://github.com/apple-oss-distributions/cctools/blob/main/include/mach-o/reloc.h).
The local load-delta contract is derived from those encodings and LLVM's actual
DWARFContext section adjustment, rather than from a global image slide.

## Exact change

`RelocationResolver.cpp` adds AArch64 unsigned fixups and includes the in-place
value for both x86_64 and AArch64. `DWARFContext.cpp` changes only MachO local
symbol calculation to start at zero, so its existing adjustment supplies the
load delta. External symbol calculation is retained.

After resolving an actual 32-bit unsigned fixup, `resolveRelocation` truncates
the final value using that actual relocation's field width. Merely promoting
the unsigned field to 64 bits is incorrect for a negative addend: a loaded
symbol `0x200` plus a raw 32-bit `-4` must produce `0x1fc`, not `0x1000001fc`.
The existing stateless resolver callback does not carry the field width.

Before `getSymbol()` can form an unchecked symbol cursor, the DWARF path checks
the actual external index. It also distinguishes local `R_ABS` from an invalid
nonzero section ordinal before dereferencing a target section. Unsupported
scattered relocations, PC-relative unsigned fixups, and unsigned fields smaller
than four bytes produce recoverable diagnostics. Paired SUBTRACTOR/ADDEND,
instruction fixups, and authenticated pointers are not admitted. The loaded
provenance collector invalidates its entire image when LLVM reports an error or
warning; it must not publish a partial address map after a diagnostic.

The proposal does not change ELF/COFF calculations or other MachO architecture
gates. It does not expose registers, locals, or guest operand stacks from line
metadata.

## Qualification inputs

`../native_macho_unsigned_relocations.s` is assembled by the actual SDK's
`llvm-mc` for x86_64-apple-darwin and arm64-apple-darwin. It contains two distinct
text sections, nonzero function offsets, local DWARF ranges, external DWARF 5
address entries, and a separate four-byte negative-addend data fixup. The data
fixup is deliberately not labeled a DWARF address section.

`../native_macho_unsigned_relocations.cc` reads these actual objects, checks the
actual raw relocation flags, and uses LLVM's DWARFContext, DWARFDataExtractor,
range parser, address parser, and resolver. Tests use independent loaded
section bases, an original base larger than its loaded base, the original-base
case, zero-load fallback, and absence of LoadedObjectInfo. A legitimate local
`R_ABS` clone retains the raw value and undefined section identity. Malformed
clones exercise diagnostics without treating unrelocated data as exact loaded
coordinates, including the AArch64 scattered-record case. The actual LLVM x64
object API does not interpret its address high bit as a scattered record, so no
x64 scattered test is invented. Structural rejection is an explicit fixture
failure; it cannot substitute for the requested DWARF error-path proof.

Keeper builds must compare the unchanged SDK baseline and the candidate SDK in
the same 64 GiB sandbox. Record actual `llvm-mc` relocation dumps, SDK hashes,
component output, and every unsupported target. Expected baseline failures are
evidence of the regression; they are not candidate passes.

Also rerun the immutable loaded-provenance r1 two-section line-table component
with ELF, COFF, and both MachO objects. Its `LoadedObjectInfo` per-section
addresses and nonzero label positions are essential: a one-section global
slide test does not qualify this fix. The actual MCJIT-owned-object component
and stopped-runtime publication/epoch tests remain distinct. Cross-reading a
MachO object on Linux is not a macOS native stepping pass.

No test binary, native execution, or compiler result was produced on the author
host. The actual cross-object and fresh runtime results are pending the sole
Linux/guest keeper.
