# Numeric conversion token spacing — 2026-10-06

Both uwvm2 and uwvm2-ros now accept whitespace between named numeric
conversion tokens and repeated whitespace between existing builtin type words.

Previously, `static_cast <int>(value)` passed DAP's syntax check as a comparison
expression, and the C++ scalar evaluator returned unavailable (status 4).
`@as (i32, value)`, `(unsigned   char)(value)` and
`sizeof(unsigned   char)` were refused by the old parsers. Compact spellings
still worked. The original C++ bridge ELF and old adapter reproduce those
observations; a separately compiled old-header control checks the defect too.

Examples of the shared read-only expression syntax:

```text
print static_cast < int > (value)
print static_cast < unsigned   char > (flag)
print (signed   char)(value)
print sizeof(unsigned   char)
print @as (i32, value)
print @as (u8, 255)
```

Guest references require an authenticated stop and available producer metadata
and storage. These are syntax examples; the tests use finite copied DATA.
This increment implements only the existing builtin numeric conversion subset.
[C++ token whitespace](https://eel.is/c++draft/lex.pptoken),
[C++ static_cast](https://eel.is/c++draft/expr.static.cast), and
[Zig grammar](https://ziglang.org/documentation/master/#Grammar)
provide the language references.

## Implementation and limits

The C++ `named_open` helper matches a name and its opening punctuation with
the existing whitespace consumer. A failed partial match restores the complete
cursor, preserving names such as `static_cast_value` and the existing variable
grammar. `builtin_type` collapses whitespace between words into one space in
a bounded owned 32-byte buffer; the normalized view does not escape.
The existing numeric aliases and conversion semantics are retained.

DAP uses the same token separation and rewind rules. It normalizes type names
only during validation and forwards the original expression unchanged.
The existing printable-ASCII single-line DAP gate remains. Whitespace cannot
join identifier fragments: `u 8` and `unsignedchar` are not builtin aliases.
Pointer types, arbitrary calls, assignment and increment remain refused.
`sizeof(unsignedchar)` may instead be an existing producer identifier, so it
is not incorrectly treated as a rejected builtin-type spelling in the tests.

The established 32-selector-step, 128-node, 32-depth and 256-byte console
bounds remain. C++ fixture strings and output use fast_io; production changes
add no filesystem or stream I/O. Guest authority, ASM scope and ROS modes
are unchanged. Concurrent unrelated DAP status/checkpoint updates are preserved;
the owned patch includes only `validate_source_evaluation_expression` in that
shared file. Both expression validator bodies equal the tested cut.

## Actual Linux qualification

All builds, executables, protocol tests, QEMU targets and property verifiers ran
inside the existing shared 64 GiB cgroup `uwvm-debug-tests64g-20261006-r2`,
with swap zero and worker CPUs 16–31, behind the owned-process supervisor.

Immutable production source cuts:

- uwvm2: `sha256:fa894f911d742ecf811b871a2e41b6c34435850d8cf06a5fd7cb9788e8e5f940`
- uwvm2-ros: `sha256:4245cf71fe46a88fff0b6682daed2c839b4a5b9da273df7b284980e881049b97`

Each of six native/QEMU jobs checks 1046 recorded expressions with a fixed
seed. The independent verifier checks acceptance, result bits, guest type
width, signedness, floating status, provider root identity, value reads and
type queries against the actual C++ ELF for both Wasm32 and Wasm64 policies.
The compiled fixture enumerates 1024 token-spacing combinations, with C++
compiler numeric conversion/sizeof oracles and variable-prefix/negative controls.

| Evidence | Result |
| --- | --- |
| Paired native phases, including old-header controls | 60 passed |
| Actual old C++ controls | 8 expected failures |
| Old DAP valid-expression refusals | 1010 reproduced |
| Generated spacing cases / actual C++ ABI rows | 6276 / 12552 |
| Compiled spacing-oracle checks | 295464 |
| DAP corpus | 247 positive / 211 negative |
| DAP frame protocol | 66 tests passed |
| Actual ppc64 big-endian / x86_64 QEMU components | 28 passed |
| Retained grouped cases / ABI rows | 6270 / 12540 |
| Retained float-range cases / ABI rows | 7380 / 14760 |
| Native separator / independent scalar checks | 1315968 / 340000 |
| Rechecked input/dependency hashes | 39550 |

Native jobs build/run twelve components and exercise four old-header failure
controls plus the DAP protocol suite. Each QEMU job covers spacing, grouped
postfix, original DWARF selectors, tuple/postfix, scalar, float range and DAP
bridge. Target output equals the separately pinned native reference.
x86_64 additionally runs the identical target ELF directly. Both guest ABI
policies run on the stated 64-bit host targets.

The first probe incorrectly expected spaced static_cast to be syntax-refused;
the corrected probe records its old comparison parse and unavailable result.
The first fixture build failed because a fast_io view has an explicit
constructor. A new immutable test cut uses that explicit constructor, with
unchanged production source IDs and verified hardlinks for unchanged files.
Both failed attempts, corrected probes, cuts and raw receipts remain retained.

## Resources and retained evidence

The six successful guards took 662.748 seconds in the shared
environment; this is not a performance benchmark. Peak aggregate owned RSS
was 789,364,736 bytes (752.8 MiB),
below 1 GiB. Maximum observed owned output was
42,994,818 bytes, below 64 MiB.
Owned trees were retired/reaped; OOM counters did not increase.

The arena has a 16 GiB hard image limit. Physical allocation after the fsynced
Linux archive was 1,503,748,096 bytes.
Existing reserves remain 6 GiB shared DATA headroom, 1 GiB real filesystem
reserve, 4096 inodes and 25 GiB host reserve; files are bounded to 8 MiB and
supervisor logs to 1 MiB. Full-product admission still requires 9 GiB.
SDK/QEMU providers are reused read-only; unrelated agents' files are preserved.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/cast-spacing-a2/cast-spacing-evidence-a1.tar.xz`

Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-cast-spacing-evidence-20261006-a1/cast-spacing-evidence-a1.tar.xz`

Archive bytes: 36,822,444; regular members: 13,370;
SHA-256: `2c2006ef5fd8b286064829964e1edd076f10c122540c2f0eee965758cccbaf8c`. Every regular member is hash-verified on both hosts
and the local copy is fsynced. Original uploads, source cuts, actual recipes,
old/current parsers, ELFs, generated cases, dependency hashes, probes, raw
receipts and logs are retained. The full prior SDK archive is rehashed;
its packages and 2777 provider files remain in the earlier dual-verified archive
with SHA-256 `fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`.
Original probe ELF provenance is retained in the prior grouped-postfix archive,
also rehashed. Final sidecars retain paired documentation and the scoped patch.

[Machine-readable qualification](debug_cast_spacing_qualification_20261006.json)
records exact hashes, cases and limits. This increment qualifies the finite
numeric syntax and component/DAP behavior above. Live producer sessions for
C, C++, Objective-C, Rust, Go/TinyGo, Zig and AssemblyScript, full CLI/module
builds, other architectures and full native debugger parity remain unqualified
by this increment.
