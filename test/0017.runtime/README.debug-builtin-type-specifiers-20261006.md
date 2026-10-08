# Standard integer type-specifier combinations — 2026-10-06

uwvm2 and uwvm2-ros now recognize the valid combinations and orders of the
existing C-family integer specifiers in numeric casts and builtin sizeof.
Previously, the fixed alias list refused `signed`, `unsigned short int`,
`unsigned long long int` and `long unsigned int`, although their shorter
canonical equivalents worked. Both old C++ bridge ELFs and old DAP adapters
reproduce four such failures with four working controls per repository.

Examples:

```text
print (signed)(value)
print (unsigned short int)(value)
print static_cast<unsigned long long int>(value)
print static_cast<long unsigned int>(value)
print (int short unsigned)(-1)
print sizeof(unsigned long long int)
```

Producer references require an authenticated guest stop with available metadata
and copied storage. Tests use finite owned DATA. The
[C++ integer type table and ordering rule](https://eel.is/c++draft/dcl.type.simple)
and [permitted combinations](https://eel.is/c++draft/dcl.type)
supply the primary grammar references. This implements integer specifier
spellings for the established scalar subset.

## Implementation

After the existing bounded whitespace normalization and alias checks, C++
counts only `signed`, `unsigned`, `char`, `short`, `int` and `long`.
Every word occurs at most once, except `long` may occur twice. Mixed signedness,
char with int/short/long, and short with long are rejected. Unknown words do not
participate in combinations. A bare `signed` means the existing 32-bit int type.

DAP expands the same finite legal declarations into their token permutations.
It preserves the original accepted scalar expression. Zig's @as type set stays
separate: new C spellings do not acquire Zig coercion authority. Existing aliases,
builtin float/Boolean behavior, variable-prefix rewind, limits and conversions
remain. The owned patch changes only the expression validator in the shared DAP
file and preserves concurrent unrelated status/checkpoint updates.

The parser retains the existing guest policy: char is signed eight-bit, short
sixteen-bit, int thirty-two-bit, long long sixty-four-bit; long follows the
authenticated Wasm32/Wasm64 policy. Host char signedness and sizeof(long) do not
replace guest type evidence. Qualification deliberately tests both guest policies
on each stated host. This is not a new 32-bit-host qualification.

Rejected examples include `signed unsigned`, `int int`, `long long long`,
`short long`, `char int`, `unsigned i32` and `long double`.
The last is a native language type outside the current IEEE32/64 subset.
Arbitrary pointers, references, arrays, function types, qualifiers and mutation
remain outside this increment. Rejected syntax clears the prior program and
runs no value/type callbacks, including dead syntax under short circuits.

Production changes add no filesystem/stream I/O. New C++ fixture strings and
output use fast_io. The 32-step, 128-node, 32-depth and 256-byte console bounds,
guest memory/stop authority, ASM scope and ROS mode selection remain.

## Actual guarded Linux tests

All compilers, executables, protocol tests, QEMU targets and property verifiers
ran inside the existing shared 64 GiB cgroup
`uwvm-debug-tests64g-20261006-r2`, swap zero, worker CPUs 16–31,
under the owned-process supervisor.

Immutable production source cuts:

- uwvm2: `sha256:100adc7fde7541f1373e627cfa3569b588c41182ac8ea1e601b0499fb3e39527`
- uwvm2-ros: `sha256:8774a5cd842f074d921c1f4a41573988d530abcd9719ec0f4454648cdc8e00b7`

The C++ fixture actually instantiates all 84 spellings and uses compiler
type-identity, fixed-width and signedness assertions against their canonical
declarations. Executed numeric/sizeof checks use compiler conversions.
Guest long values use explicit fixed-width compiler types, and plain char
retains the existing guest signedness policy. Host type identity and guest
numeric policy are recorded separately.

An independent generator starts from the standard canonical declarations and
enumerates their spelling permutations. Every native/QEMU job checks 1803
recorded expressions: all 84 spellings with four conversion/sizeof modes and
two whitespace forms, 1024 seeded additional cases, four variable-name controls
and 103 malformed/unsupported cases. Actual C++ result rows check parse and
evaluation status, result bits, width, signedness, floating status, root identity,
value reads and type queries for Wasm32 and Wasm64.

| Evidence | Passed result |
| --- | --- |
| Paired native phases, including old-header controls | 64 |
| Actual old C++ failure controls / old DAP valid-case refusals | 8 / 2746 |
| Standard type spellings instantiated by each compiler | 84 |
| Type-specifier case checks / actual C++ ABI rows | 10818 / 21636 |
| Compiled type and retained spacing oracle checks | 311592 |
| DAP corpus | 255 positive / 223 negative |
| DAP frame protocol tests | 66 |
| Actual ppc64 big-endian / x86_64 QEMU components | 32 |
| Retained spacing cases / ABI rows | 6276 / 12552 |
| Retained grouped cases / ABI rows | 6270 / 12540 |
| Retained float-range cases / ABI rows | 7380 / 14760 |
| Native separator / scalar property checks | 1315968 / 340000 |
| Rechecked input/dependency hashes | 43060 |

Each native job builds/runs thirteen components, four old-header failure
controls and the DAP protocol suite. Each QEMU job runs the new type fixture,
spacing, grouped postfix, DWARF selectors, tuple/postfix, scalar, float range
and DAP bridge. All target default outputs equal a separately pinned native
reference. x86_64 also executes its identical target ELF directly.

The first old-version probe stopped before running an ELF because the new
private output parents were absent. After creating only those two directories,
the unchanged pinned probe passed for both repositories. Initial receipts,
corrected observations and actual old ELFs/provenance remain retained.

## Resources and evidence

Successful guards took 738.978 seconds in the shared environment;
this is not a benchmark. Peak aggregate owned RSS was
792,383,488 bytes (755.7 MiB),
below 1 GiB. Maximum observed owned output was
50,071,482 bytes, below 64 MiB.
All owned trees were retired/reaped and OOM counters did not increase.

The arena retains a 16 GiB hard image limit. Physical allocation after the
fsynced Linux archive was 1,763,397,632 bytes.
Admission/reserves remain 6 GiB shared DATA headroom, 1 GiB real filesystem
reserve, 4096 inodes and 25 GiB host reserve. Per-file/log bounds remain
8 MiB/1 MiB; full-product admission still requires 9 GiB. Existing SDK/QEMU
providers are reused read-only and rehashed; unrelated agents' files remain.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/builtin-type-a1/builtin-type-evidence-a1.tar.xz`

Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-builtin-type-evidence-20261006-a1/builtin-type-evidence-a1.tar.xz`

Archive bytes: 26,709,188; regular members: 7,212;
SHA-256: `47dcfe6f70156933d3231add1952936a58aeb5d945685456070fca9b109669d5`. Every regular member is hash-verified on both hosts
and fsynced locally. Source uploads/cuts, old/current parsers/adapters, actual
recipes, dependency records, native/target ELFs, generated cases, probes,
raw receipts/logs and the scoped source review are retained. The existing
full SDK archive is rehashed, as are all 2777 provider files before/after;
packages/contents remain in the prior dual-verified provider archive
`fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`.
The prior cast-spacing archive is also rehashed for original ELF provenance.
Final sidecars retain paired documentation, the scoped patch and copy proofs.

[Machine-readable qualification](debug_builtin_type_specifiers_qualification_20261006.json)
records exact inputs and limits. This increment qualifies the finite type
spellings and component/DAP behavior above. Live producer sessions for
C/C++/Objective-C, Rust, Go/TinyGo, Zig and AssemblyScript, full CLI/module
builds, other architectures and full native debugger parity remain unqualified
by this increment.

After dual archive validation and fsync, only the two redundant local source-upload archives from this round were removed (9,684,928 bytes). Their originals remain on Linux and in the verified archive; prior archives and other agents files remain preserved.
