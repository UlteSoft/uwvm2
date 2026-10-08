# Standard integer rank and copied primitive result names — 2026-10-07

Both `uwvm2` and `uwvm2-ros` now preserve the standard C/C++ integer type
identity of `int`, `unsigned int`, `long`, `unsigned long`, `long long` and
`unsigned long long` in the finite scalar evaluator. Validated declarations,
standard casts and finite literal candidates propagate that identity through
promotion, unary operations, shifts, usual arithmetic conversions and numeric
conditionals. Canonical copied result names distinguish equal-width types.
C/Objective-C and C++/Objective-C++ use the selected actual CU language;
explicit C23 selection retains its earlier version boundary.

On the supported Wasm32 ABI, `int` and `long` are both 32-bit, but `long` has
greater conversion rank. The old width/sign representation gave the numeric
values in these examples but lost their standard result type. This step fixes
that type identity/name loss; it does not claim an old numeric-value mismatch.

| Expression | Wasm32 copied type | Guest64 numeric model copied type |
|---|---|---|
| `1` / `1U` | `int` / `unsigned int` | same |
| `1L` / `1LL` | `long` / `long long` | same, with 64-bit long |
| `(int)1 + (long)2` | `long`, signed 32-bit | `long`, signed 64-bit |
| `(unsigned int)1 + (long)2` | `unsigned long`, 32-bit | `long`, signed 64-bit |
| `(unsigned long)1 + (long long)2` | `long long`, signed 64-bit | `unsigned long long`, 64-bit |
| `1 ? (long)1 : (unsigned int)2` | `unsigned long` | `long` |
| `(long)1 << (int)2` | `long` | `long` |
| `(short)1 + (long)2` | `long`, after short-to-int promotion | `long` |
| `(i32)1 + (int)2` | generic signed integer; alias syntax does not establish rank | same |
| `sizeof(int)` | generic unsigned integer; no guessed `size_t` identity | same |

Use the actual thread/stop identifiers at an existing authenticated source pause:

```text
print THREAD STOP (int)1 + (long)2
print THREAD STOP (unsigned int)1 + (long)2
print THREAD STOP 1 ? left : (long)2
print THREAD STOP (long long)1 + (unsigned long)2
```

`left` must be a supported variable in the selected source frame with valid
embedded type metadata. The guest ABI controls `long`; the debugger host's
width does not. The guest64 column is finite numeric DATA qualification;
the fresh Wasm metadata samples here use Wasm32. This cut compiles the actual
production controller frontend, but does not link a fresh full product or
qualify a live `-Rdbg` stop/print session. Earlier product evidence retains
its original source identity.

## Implementation and boundaries

A separate immutable standard-integer marker records conversion rank instead
of conflating it with representation width. The actual LLVM parser requires
a bounded direct `DW_TAG_base_type` name, accepted C/C++ CU language,
signed/unsigned encoding and a supported guest extent. `int` is four bytes,
`long long` eight; `long` matches the module's four/eight-byte address ABI.
Type aliases retain their own display names but derive rank only from the
actual final base DIE. Named typedefs, matching widths or spelling alone do
not establish rank. Atomic wrappers, pointers, enums, TinyGo C99 producers,
non-C/C++ producers and cross-CU wrapper chains do not gain that marker.

For C-family non-C++ units, the extra bounded base-name query is restricted
to four/eight-byte scalars. Existing narrow C-type query costs remain. C++
uses the existing bounded base-name route. Limits on strings, references,
DIEs and input sizes remain. Synthetic actual LLVM units cover misleading
alias names, atomic wrappers inside aliases, wrong guest-long extents and
Go/TinyGo exclusions, alongside fresh compiler metadata.

Both controller paths attach rank and canonical type keys from the actual
selected immutable declaration, using the authenticated module's address
width. The coherent memory path can query the root scalar's type metadata
for wide integers as well as narrow ones; that query does not evaluate a
location or issue a guest read. If proof is unavailable, the existing finite
numeric result stays generic. The copied-local path still has no memory
reader. Existing frame/participant/pause/generation checks remain. No native
pointer, callable, writable C++ lvalue, VM read permission or new read token
is introduced. ASM remains confined to generated Wasm contexts.

Known standard operands use conversion rank for the common type: same-sign
operands choose the greater rank; mixed signedness chooses the unsigned type
when its rank is at least the signed type's, otherwise the signed type if it
represents all values, otherwise the signed type's unsigned counterpart.
Unknown ranks retain the old generic numeric path. A same-DIE conflict between
known standard markers returns unavailable before conditional value reads.
Selected copies must retain their inferred standard marker and exact canonical
DIE when present. A computed common type retains a DIE only when both operands
prove that exact key and primitive identity. Declaration keys remain type DATA,
not object addresses or location authority.

Narrow integer and Boolean promotion yields standard `int` in the native
C/C++ profiles. Standard casts/specifier permutations, ordinary integer
suffixes and finite decimal/nondecimal candidate rules carry their respective
rank. Shared numeric aliases and the default shared API do not acquire native
wide markers. Integer unary and shift results retain the promoted left type;
arithmetic/bitwise and conditional results retain a proven common type.
Existing float/double/Boolean names and shared numeric behavior remain.
Copied names are still built with `fast_io::concat_std`; C++ test IO and
existing numeric scanning use fast_io.

This is canonical primitive copied-value naming, not complete native `ptype`,
CV/reference/glvalue/typedef spelling or class reconstruction. `sizeof`
results retain a generic unsigned name until actual `size_t` identity is
available. Extended `_BitInt`/integer types, long double, wider/UTF character
literals, all producer ABI options, scoped-enum/overload/pointer rules and
complete native expression grammar remain unfinished. An unsuffixed decimal
outside signed 64-bit standard candidates keeps the historical finite value
without fabricating an extended native type name. Complete literal semantics
are not claimed. C23 samples here still emit older CU language codes
`[29]`;
automatic C23 version selection remains incomplete. Full Rust, standard Go,
TinyGo, Zig and AssemblyScript language evaluators/live producer sessions
remain outside this qualification.

The conversion rules were checked against
[C++ usual arithmetic conversions](https://eel.is/c++draft/expr.arith.conv),
[integer conversion ranks](https://eel.is/c++draft/conv.rank),
[integer literal candidates](https://eel.is/c++draft/lex.icon), and
[C23 arithmetic conversions](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf).
These references support the finite standard subset, not all native debugger
capabilities.

## Verification

All compilation, executable runs, provider restoration and functional Python
verification ran on SSH Linux inside the existing 64 GiB/swap-0 cgroup.
Boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`, keeper PID/birth `9769/16929`.
Birth/PIDFD ownership confines supervision and retirement to each job's own
process tree. Administrative capture, report/JSON writing, hashes and archive
checks used no local executable tests. Existing peer edits/processes/evidence
were preserved.

| Check | Passed |
|---|---:|
| Guarded compiler/component/metadata/frontend jobs | 12 |
| Native compiler/scalar/protocol phases | 68 |
| Actual LLVM metadata and fresh Wasm compiler phases | 48 |
| Actual production controller frontend, uwvm-int and LLVM | 4 |
| Fresh embedded-DWARF Wasm modules | 40 |
| Independent Wasm C/C++ static rank type assertions | 320 |
| Actual parser/producer assertions | 7200 |
| Real standard wide integer metadata proofs | 280 |
| Standard rank/name positive and negative assertions | 855880 |
| Narrow bridge/name regression assertions | 482344 |
| Exact narrow/C23 regression assertions | 140984 |
| Actual QEMU components equal to pinned native output | 66 |
| Independent C17/C23 native type assertions | 44 |
| Independent C17/C23 native runtime checks | 3252 |
| Exact old generic computed names reproduced and fixed | 10 |
| Canonical standard integer CLI name comparisons | 10 |
| DAP frame protocol tests | 66 |
| Integer/IEEE-f32 scalar property comparisons | 1360000 |
| Input hash checks | 145657 |

Each new fixture execution passes 106,985 checks. It compares all 36 pairs of
six standard integer families against native `decltype` and runtime values
for addition, bitwise OR and both conditional arms, C/C++ profiles and values
-3 through 3. An independent ILP32 conversion table covers the equal-width
int/long case with C/C++/C23 profiles. Literal/suffix/specifier names, unary
and shifts, promotions, unknown aliases, invalid rank markers, dead-arm
non-evaluation, atomic/TinyGo/Go/cross-CU/ABI exclusions, conflicting keys and
selected-copy mismatch are checked. Original generic names are reproduced
using exact retained previous scalar/type headers and observed dependencies;
no simulated old implementation or old numeric mismatch is claimed.

Each repository compiles 20 fresh C17/C23/C++20/Objective-C/Objective-C++ Wasm
modules across DWARF4/5 and O0/O2. Eight compiler type assertions per module
provide independent Wasm32 conversion witnesses. The actual LLVM parser then
checks seven wide variable/alias functions per module, yielding 140 standard
wide metadata proofs per repository. Previous narrow primitive proofs and
C exclusions are retained. Finite copied callbacks demonstrate metadata-driven
typing; they are not live authenticated guest pause/read evidence.

PPC64 big-endian and x86_64 QEMU each run eleven components in both repositories.
Aarch64 QEMU additionally runs that same current source cut after bounded SDK
restoration. Target ELF machine/width/byte order and pinned native stdout are
checked. Full product/native hardware/all-architecture parity is not claimed.
The actual production controller frontend compiles in uwvm-int/JIT-off and
LLVM/JIT-only macro families; ROS mode scope remains unchanged.

Aarch64 restoration uses eight fixed official APT package identities downloaded
from the [Ubuntu GCC cross-toolchain archive](https://archive.ubuntu.com/ubuntu/pool/main/g/gcc-15-cross/)
and [cross sysroot archive](https://archive.ubuntu.com/ubuntu/pool/main/c/cross-toolchain-base/),
and verified against the cached package SHA-256 and
size. Files are extracted into an isolated provider directory without system
installation or maintainer script execution. The restored compiler/runtime
providers and QEMU identity are rehashed before/after tests. Provider evidence
is retained in the main archive. Remaining architecture sysroots are still
missing. See the archived provider plan, restoration record and guard receipts
for exact package versions and byte limits.

Successful guarded work totaled 1559.51 seconds,
a sum of job work, not wall time/performance. No failed functional attempts
occurred in this cut. DATA peak own RSS was 811524096 bytes
(773.93 MiB), within its unchanged 1 GiB limit.
Frontend peak was 2194673664 bytes
(2.04 GiB), within the separate original
16 GiB compiler budget. Main and aarch64 component output directories each
have independent unchanged 64 MiB own-output/8 MiB per-file limits. Provider
restoration has a separate 160 MiB own-output bound, 16 MiB download and
112 MiB extracted-regular-file bounds, and 32 MiB per-file limit, lower than
the preceding provider class's limits. All use the same hard 16 GiB ext4 arena,
existing inode/host disk reserves and cgroup. OOM counters did not increase.
The functional collector is separately guarded after all jobs.

## Evidence and remaining work

Ordinary immutable source cut: `sha256:b2f15aef7e2e996d21ee22b28b0be1906ad8c3647dd2cd92fb087137125a85ce`.
ROS immutable source cut: `sha256:6bf2d3664efc1120f06d8f5d1cae51754ed20bbdc94aec6c7d72524a9ab3569d`.
The ten owned implementation/fixture/registry paths equal their tested cuts
and match across repositories. Inventory updates retain all historical fields
and all other features; full native debugger capability status remains partial.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/integer-rank-a1/integer-rank-evidence-a1.tar.xz`.
Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-integer-rank-evidence-20261007-a1/integer-rank-evidence-a1.tar.xz`.
Archive 58786388 bytes, 9752 regular members,
SHA-256 `91ccf0e59c14df364fbb019e14cbfbb733b5c7b3a0ede365ca5a94b527e62973`. Every member hash was verified on both hosts and
both archives fsynced. Sources, exact prior headers, target/native binaries,
Wasm samples, dependencies, pinned recipes and receipts are retained.
Installed LLVM and the earlier PPC provider archive remain external providers
with recorded identities; the archive is not a complete SDK/full-product
qualification. Final paired patches/documents are retained in a separate
member-verified dual-copy sidecar.

After archiving, the hard 16 GiB image occupied
3173388288 physical bytes
(2.955 GiB).
Machine-readable evidence and unfinished scope are in
`debug_integer_rank_qualification_20261007.json`.
