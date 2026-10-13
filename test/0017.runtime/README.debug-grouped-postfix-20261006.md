# Grouped producer-reference postfix composition — 2026-10-06

Both uwvm2 and uwvm2-ros now preserve an existing producer reference through
parentheses when it is followed by member access, indexing, arrow access or
Zig postfix dereference inside scalar expressions. Previously, `(pair).0`
passed DAP's standalone selector validation but failed C++ scalar parsing;
`(*p).field + 1` was refused by both.

Examples of the shared read-only syntax:

```text
print (*p).field + 1
print ((pair)).0 + 1
print ((p)).* + 1
print ((array)[1]).field + 1
print (packet)->field + 1
print (value) - 1
```

These commands require an authenticated guest stop and available producer
metadata/storage. They are syntax examples; the test providers use finite
owned DATA. Missing or unavailable fields still return unavailable.

The C++ draft specifies that grouping preserves an expression's type, result
and value category, and defines member access on postfix expressions.
This change implements the existing producer-reference subset.
[C++ parentheses](https://eel.is/c++draft/expr.prim.paren),
[C++ member access](https://eel.is/c++draft/expr.ref).
[GDB's C/C++ operator documentation](https://sourceware.org/gdb/current/onlinedocs/gdb.html/C-Operators.html)
is the native reference; this increment does not establish full GDB parity.

## Implementation

`source_dwarf_expression.h` factors the existing bounded loop into
`postfix_tail`. The scalar caller leaves a non-arrow minus for arithmetic;
standalone selector behavior is unchanged. The existing named/numeric member,
signed decimal index, arrow and Zig dereference parsing is reused.

`source_scalar_expression.h` continues a grouped result only when its AST
node is an existing reference. It clones that reference, appends the complete
tail, and submits one resulting path. Without a tail it reuses the original
node. DAP implements corresponding continuation and combined-step accounting.

The existing 32-step/128-node/32-depth and 256-byte console bounds remain.
C++ production output/scanning uses fast_io. Existing reference-owned strings
are preserved; no new filesystem or stream I/O is introduced in production.
Guest stop authority, memory authority, ASM scope and ROS mode selection are
unchanged.

These are still refused, including inside dead syntax:

```text
(p + 1)->field + 1
(42).field + 1
(true).field + 1
(int(p)).field + 1
(sizeof(p)).field + 1
(1 ? p : p).field + 1
(packet)[1 + 1] + 1
(packet).field = 1
(packet).field++
```

Arithmetic, literal, cast, sizeof and conditional numeric results cannot
acquire a new access path. Rejected syntax clears the prior program and invokes
neither value nor type callbacks. Short-circuit tests such as
`0 && ((*p).field + missing)` parse the full syntax but invoke no skipped
resolver. Expression arithmetic does not generate a guest/host pointer.

DAP's existing canonical decimal-index rule remains. C++ standalone selection
previously accepted leading-zero indices; that retained difference is explicitly
excluded from the cross-parser equality claim.

## Actual Linux qualification

All compilers, test executables, protocol tests, QEMU targets and property
verifiers ran in the existing shared 64 GiB cgroup
`uwvm-debug-tests64g-20261006-r2`, swap zero, worker CPUs 16–31,
behind the original owned-process supervisor.

Immutable production source cuts:

- uwvm2: `sha256:c847e8f2712743d9440850adb6d086a837326ef79b6a6a3f00c9d3292f7e4249`
- uwvm2-ros: `sha256:ceb9d92c4d169fec393b06de637e6873be16452d7cccec7de276c0800e9d79ce`

The public paired QEMU registry includes `debug_source_grouped_postfix` and its
independent verifier. Each of six native/QEMU jobs checks 1045
grouped expressions: a fixed-seed generator records the root and every member,
signed index and dereference in order. An independent path signature, result
bits/width, read counts, zero type queries, short circuits, rejected paths and
combined selector limits are checked against actual C++ ELF output.

| Evidence | Result |
| --- | --- |
| Paired native phases, including old-header controls | 54 passed |
| Actual old C++ parser refusals | 6 expected refusals |
| Old DAP valid-path refusals | 2052 reproduced |
| Grouped case checks | 6270 |
| Actual C++ Wasm32/Wasm64 result rows | 12540 |
| DAP corpus | 239 positive / 201 negative |
| DAP frame protocol | 66 tests passed |
| Actual ppc64 big-endian and x86_64 QEMU components | 24 passed |
| Retained float-range cases / ABI rows | 7380 / 14760 |
| Retained native separator checks | 1315968 |
| Independent native scalar property checks | 340000 |
| Rechecked input/dependency hashes | 36068 |

Each native job covers grouped postfix, original DWARF selectors,
tuple/postfix, DWARF expression, scalar, conditional, property, float range,
separator, Boolean and DAP bridge components, plus three old-header negative
controls and 33 DAP frame tests. Each QEMU job covers grouped postfix, DWARF
selectors, tuple/postfix, scalar, float range and DAP bridge. All target outputs
equal the separately pinned native reference; x86_64 also runs its identical
target ELF directly as a control. Both guest ABI policies run on the stated
64-bit host targets; this is not a new 32-bit-host qualification.

The initial cross attempt stopped before compilation because the private
snapshot manifest used `production_files` and the public recipe required
`files`. A new cut adds that alias and uses verified hardlinks for unchanged
source bytes. Two passed native jobs are reused and four cross jobs ran on the
corrected manifest. All initial receipts, cuts and the correction note remain
retained. Production inputs, actual compiler dependency files, link providers,
target ELFs and logs are rechecked afterward.

## Resources and evidence

Successful guards took 604.411 seconds in the shared environment,
not a benchmark. Peak aggregate owned RSS was 788,803,584 bytes
(752.3 MiB), below 1 GiB. Maximum observed test
output was 22,810,405 bytes, below 64 MiB.
All owned trees were retired/reaped and OOM counters did not increase.

The ext4 arena retains its 16 GiB hard image limit. Physical allocation after
archiving was 1,278,763,008 bytes. Existing
admission/reserves remain: 6 GiB shared DATA headroom, 1 GiB real filesystem
reserve, 4096 free inodes, 25 GiB host reserve, 8 MiB files and 1 MiB supervisor
logs. Full-product admission still requires 9 GiB. SDK/QEMU providers are reused
read-only; unrelated agents' files are preserved.

Linux archive:

`/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/grouped-selector-a2/grouped-postfix-evidence-a1.tar.xz`

Local archive:

`/Users/liyinan/.codex/artifacts/uwvm2-grouped-postfix-evidence-20261006-a1/grouped-postfix-evidence-a1.tar.xz`

Archive bytes: 36,455,416; regular members: 13,255;
SHA-256: `c347cebe76bcea5fec700fa97ff52c501c13885314d26bcad830ff8cce887195`. Every regular member is hash-verified on both hosts;
the local copy is fsynced. Both immutable cuts, original uploads, actual
recipes, native/target ELFs, generated cases, old parsers/adapters, raw receipts,
logs and scoped paired source review are retained. The SDK regular files and
the prior full provider archive are rehashed; its packages/contents remain in
the earlier dual-verified archive with SHA-256
`fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`.
Final sidecars retain paired documentation, the final scoped patch and copy proofs.

[Machine-readable qualification](debug_grouped_postfix_qualification_20261006.json)
contains exact hashes and limits. This increment qualifies syntax, read plans
and the stated component/DAP behavior. Live C/C++/Objective-C, Rust, Go/TinyGo,
Zig and AssemblyScript producer sessions, full CLI/module builds, other
architectures and full native debugger parity remain unqualified by this increment.

Concurrent unrelated DAP status and WASIp1 checkpoint changes are preserved. The final owned patch contains only the expression validator change in that shared file. Both current expression validator bodies are byte-identical to the tested cut; the complete current adapter is not claimed to be identical to that cut.

The first local archive transfer hit transient ENOSPC after passing its admission reserve. Only this incomplete transfer and two SHA-verified redundant upload archives already retained on Linux were removed. The retry was fully hash-verified and fsynced; prior archives and other agents' files remain preserved.
