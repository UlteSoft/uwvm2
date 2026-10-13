# Decimal floating separators — 2026-10-06

Both repositories now accept apostrophes and underscores between successive
decimal digits in the integer, fraction and exponent portions of the shared
read-only scalar expression grammar. Examples are `1'234.5`, `1_234.5`,
`.1_25f` and `0.15e+0_2`. The ordinary unsuffixed f64 and f/F-suffixed
f32 rules are unchanged.

[C++ floating literal grammar](https://eel.is/c++draft/lex.fcon) permits
apostrophes in digit sequences, including the exponent.
[Go floating literal grammar](https://go.dev/ref/spec#Floating-point_literals)
permits underscores between successive decimal digits. This is their shared
finite numeric syntax; accepting both separator styles does not establish
full per-language expression or producer debugging semantics.

The production parser validates each digit segment and normalizes only owned
syntax into a bounded 4096-byte buffer. Conversion still uses
`fast_io::parse_by_scan` directly into float or double, preserving the
existing f32 rounding and finite-literal checks. Existing integer base-prefix,
separator and candidate-type rules retain their old path. No guest read,
address capability, write, call or runtime stop is created by literal parsing.

The DAP validator accepts the same separated decimal forms, validates the
normalized finite value and preserves the original expression for the
authenticated controller query. Separator placement beside a decimal point,
exponent marker/sign or suffix is rejected, including syntax in a dead
conditional or logical operand. Extended floating suffixes, hexadecimal
floats and other previously unsupported numeric forms remain outside this
increment.

The public Linux QEMU recipe now registers both the new
`debug_source_float_separator` and the existing
`debug_source_scalar_property` component. The same implementation,
fixture, DAP corpus and registry edits are applied to uwvm2 and uwvm2-ros.

## Actual fixed-source qualification

| Actual run | uwvm2 | uwvm2-ros |
| --- | --- | --- |
| Native component regression | 29 / 29 recorded phases | 29 / 29 recorded phases |
| ppc64 ELF64, big endian, QEMU | 7 / 7 components | 7 / 7 components |
| x86_64 ELF64, little endian, QEMU | 7 / 7 components | 7 / 7 components |

All six jobs passed. The phase ledger has 128 rows, including the eight
expected old-header refusals. Each cross target also runs Boolean casts,
Zig categories, ordinary scalar expressions, random scalar properties,
conditionals and the DAP expression bridge. Every actual QEMU output equals
the same repository's pinned native reference. x86_64 QEMU additionally
compares directly executed target ELF output.

The new fixture performs 657,984 checks per primary execution and 3,947,904
across the six primary executions. Its independent value oracle is actual
compiler-produced C++ floating constants, including native apostrophe
literals. Sixteen f32/f64 samples include maximum finite, minimum normal,
minimum subnormal, integer precision boundaries and exponent forms.
For each of 512 deterministic separator layouts per sample, both Wasm32 and
Wasm64 guest ABI settings exercise plain literals, unary minus, arithmetic,
selected/dead conditionals and short-circuit expressions. Bit patterns,
widths, floating/signed flags and zero resolver reads/queries are checked.
Malformed separators are also rejected inside dead syntax.

The actual old scalar header
`fdce64b36041d88e109591eb7680f05c321b8c9e9089658a12b2fdc2b4768c5e`
fails four representative separated literals in each repository while
keeping the surrounding current source cut fixed. The old DAP adapter
rejects all 19 newly added positive cases in each repository. These are
recorded expected failures, rather than skips.

The DAP corpus has 221 positive and 132 negative expressions, with 19 new
positive and 72 new negative cases. Both real C++ bridge results and the
actual Python validator are verified. In total 73,493 input-hash checks
completed, with source, dependency, link-provider, QEMU, ELF and log hashes
verified after the runs.

Successful production source IDs are unchanged across all three immutable
attempt directories:

- uwvm2: `sha256:1f73d445893dd93e628ff51a6c67231d5fd93a9fe0c2dcefb8ebbb1ad9161786`
- uwvm2-ros: `sha256:5b0e2b2e823c60652c69dfaec43b2c4f74ce26112ee005d1adb7af5ce69a4a06`

The first new fixture build used raw C string arguments unsupported by the
fast_io concat API. A fresh fixture revision uses explicit string views.
The next existing regression needed the already captured Boost include root,
so its keeper recipe was corrected and restarted in a new output directory.
The first cross attempt rejected the unregistered scalar-property case
before invoking a compiler; the paired public registry was corrected in
a new immutable recipe cut. All three failed guard receipts and raw
diagnostics remain retained. Verified immutable source hardlinks avoid
duplicating those production cuts or changing another agent's work.

## Resource bounds and durable evidence

Every compiler and executable invocation used the existing shared
`uwvm-debug-tests64g-20261006-r2` cgroup, memory.max 64 GiB, swap.max zero,
and CPUs 16–31 for test workers. Six passed guarded jobs took 725.052 seconds
in total. These are shared-environment observations, not performance scores.

Peak aggregate owned RSS was 772,313,088 bytes, under the 1 GiB DATA limit.
Maximum observed output in a guarded writer root was 29,793,930 bytes,
under 64 MiB. No OOM events increased, and all owned process trees were
retired/reaped. File/log limits remain 8 MiB/1 MiB, with 1 GiB real filesystem
reserve, 4096 free inodes, 25 GiB host disk reserve and 6 GiB shared DATA
admission headroom. Full-product admission still requires 9 GiB headroom.

The persistent ext4 arena retains its 16 GiB hard image limit. It allocated
798,650,368 bytes before this round's evidence compression, including
previous retained work. The pinned ppc64 SDK and QEMU providers are reused
read-only. SDK packages and regular files are in the prior independently
verified archive with SHA-256
`fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`;
this round includes its reference proofs and rechecks the actual SDK file
hashes rather than downloading or copying the SDK again.

Main Linux archive:
`/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/float-separator-a3/float-separator-evidence-a1.tar.xz`

Local copy:
`/Users/liyinan/.codex/artifacts/uwvm2-float-separator-evidence-20261006-a1/float-separator-evidence-a1.tar.xz`

It contains 19,453 regular members, is 44,957,744 bytes, and has SHA-256
`18dea54e9f0a6701ef8561a7f4df248f63ed587988531fd0cb8d8c031e026abc`.
Every member hash was verified on Linux and locally. It retains the
three cuts, actual tool recipes, native/target ELFs, all logs and receipts,
old scalar/DAP inputs, source diff/review and copied ppc64/x86_64 QEMU
binaries. Final sidecars retain paired report/guide/source review and
archive-copy proofs.

[Machine-readable qualification](debug_float_separator_qualification_20261006.json)
records exact hashes, cuts and guard outcomes. This round establishes
the stated syntax/numeric/DAP DATA behavior on native x86_64 and actual
ppc64/x86_64 Linux QEMU components. It does not qualify live C/C++/Objective-C,
Rust, Go/TinyGo, Zig or AssemblyScript producer sessions, full CLI/module
builds, all other architectures or complete native language debugger parity.

At a genuinely authenticated existing source stop, expression forms include:

```text
print-frame THREAD STOP FRAME 1'234.5
print-frame THREAD STOP FRAME .1_25f
print-frame THREAD STOP FRAME real32 + 1_000.5f
print-frame THREAD STOP FRAME 1 ? 0.15e+0_2 : 7.0
```

Replace THREAD, STOP and FRAME with the actual current identities.
The original stopped-frame and copied-value authentication still applies.
