# C++ narrow variable/cast matching and copied primitive names — 2026-10-07

Both `uwvm2` and `uwvm2-ros` now match an actual C++/Objective-C++ narrow
variable with a standard builtin cast when bounded DWARF base metadata
proves the same primitive type. A copied expression result displays `char`,
`signed char`, `unsigned char`, `short` or `unsigned short` when that primitive
identity is known. This closes the previous variable-versus-builtin and
independent validated base-DIE gaps in the finite numeric evaluator.

| Example in a selected C++ source frame | Copied result |
|---|---|
| `1 ? left : (short)2`, `left` declared as `short` with value -1 | signed 16-bit -1, type name `short` |
| `0 ? (signed short int)2 : right`, `right` declared as `short` with value -1 | signed 16-bit -1, type name `short` |
| `1 ? (1 ? left : (short)2) : (short)3` | signed 16-bit -1, type name `short` |
| `1 ? left : right`, both validated standard `short` but different base DIE keys | signed 16-bit result, type name `short`, no invented common DIE key |
| `1 ? left : (i16)2` | unavailable: shared numeric alias spelling does not prove a C++ builtin type |
| `1 ? (char)1 : (signed char)2` | signed 32-bit integer promotion; `char` and `signed char` are distinct types |
| `1 ? +(short)1 : +(short)2` | signed 32-bit integer promotion; narrow result name cleared |

For an existing authenticated pause, use the current thread/stop identifiers:

```text
print THREAD STOP 1 ? left : (short)2
print THREAD STOP 0 ? (signed short int)2 : right
print THREAD STOP 1 ? (1 ? left : (short)2) : (short)3
```

The examples require the corresponding selected source frame, supported
variable locations and C++/Objective-C++ CU language metadata. This cut
qualifies finite metadata and copied numeric DATA, plus actual production
controller frontend compilation. It does not qualify a fresh linked full
product or live `-Rdbg` stop/print session. Earlier full-product evidence
retains its original source identity.

## Type proof and implementation

The actual LLVM parser preserves a finite standard primitive marker only
from an actual final `DW_TAG_base_type`. Its direct bounded base name,
CU language, DWARF encoding and byte extent must agree. A typedef display
name never substitutes for the base name. A synthetic alias named `char`
whose actual base is `short` is classified as `short`; an unsupported base
name never acquires a standard identity merely from width/sign. A wrapper
crossing CU identity remains conservative. Existing parser/reference/string
budgets remain; the extra direct base-name query is restricted to non-atomic,
non-pointer C++ same-CU types. Other language producers incur no extra query.

Supported standard primitives on the finite default signed-char Wasm ABI:
`char` and `signed char` use DW_ATE_signed_char/one byte; `unsigned char`
uses DW_ATE_unsigned_char/one byte; `short` uses DW_ATE_signed/two bytes;
`unsigned short` uses DW_ATE_unsigned/two bytes. Bounded observed standard
short spellings normalize to the corresponding type. `char` retains its
own type identity. Unsigned plain-char producer options and other character
ABIs/literals remain unqualified.

Both existing controller scalar paths use the same immutable type helper.
It attaches a canonical declaration DIE key only to a valid non-atomic
numeric scalar with matching extent and signed/unsigned encoding. An atomic
wrapper is tracked even when hidden inside a named typedef, fixing the
previous possibility that alias display qualifiers hid that property.
Such atomic types do not acquire narrow identity proof. The helper creates
no native pointer, location evaluator, memory reader or authentication token.
The coherent memory-transaction path and copied-local path retain their
existing frame, participant, generation and pause checks. ASM remains
restricted to generated Wasm contexts and cannot inspect the VM itself.

Conditional inference still queries both operand types before reading a
value and evaluates only the selected arm. Matching validated standard
primitives can bridge a builtin cast or independent base DIEs; matching
canonical keys remain available where no standard marker is known. Equal
width/sign/display-name alone does not establish identity. Conflicting
known primitive markers on the same DIE are rejected before value reads.
The actual selected copy must still match the inferred selected declaration's
exact DIE key when present; a matching primitive cannot bypass that check.
A computed common type retains a DIE only when both inferred operands share
that exact key. Nested primitive results therefore keep a standard type
without inventing an operand's declaration key.

The controller constructs the copied result name with `fast_io::concat_std`.
Existing Boolean/float/double names remain. These canonical primitive names
are not full native `ptype`, typedef spelling, CV/reference/glvalue or class
reconstruction. Numeric promotion clears a narrow primitive marker.

C/Objective-C/C23 integer promotion remains unchanged. The parser does not
grant C++ primitive identity to C, Rust, Go, TinyGo or unknown producers.
Fresh C23 modules here still report old CU language codes
`[29]`;
automatic C23 version selection remains incomplete. Previous explicit
`DW_LANG_C23` Boolean qualification is retained. No fresh TinyGo, standard
Go, Rust, Zig or AssemblyScript producer/live session is qualified here.
Full enum/pointer/class/overload conditional rules, native collections,
traits/goroutines/dynamic dispatch and full language parsers remain unfinished.

Type rules were checked against [C++ conditional expressions](https://eel.is/c++draft/expr.cond)
and [C++ fundamental types](https://eel.is/c++draft/basic.fundamental).
Encoding/language constants were checked against
[LLVM's DWARF definitions](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/BinaryFormat/Dwarf.def).

## Verification

All compilation, executable tests and functional Python verification ran on
SSH Linux inside the existing 64 GiB/swap-0 cgroup. Boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`,
keeper PID/birth `9769/16929`. A birth/PIDFD-bound
supervisor owns and retires only its own process tree. The bounded 16 GiB
ext4 arena is reused. No peer edits, foreign process trees or earlier evidence
were removed. Administrative source capture, JSON/report writing, hash and
archive checks used no local executable tests.

| Check | Passed |
|---|---:|
| Guarded build/test jobs | 10 |
| Native compiler/scalar/protocol phases | 64 |
| Actual LLVM metadata and fresh Wasm compiler phases | 48 |
| Actual controller frontend paths, uwvm-int and LLVM | 4 |
| Fresh embedded-DWARF Wasm modules | 40 |
| Actual parser/producer assertions | 4290 |
| Real C++/Objective-C++ primitive bridges | 96 |
| Real C-family primitive exclusions | 144 |
| Existing canonical narrow DIE proofs | 16 |
| New primitive bridge/name assertions | 361758 |
| Exact narrow/C23 regression assertions | 105738 |
| Numeric language regression assertions | 65796 |
| QEMU components matching pinned native output | 40 |
| Independent C17/C23 native type assertions | 44 |
| Independent C17/C23 native value checks | 3252 |
| Exact old variable/cast refusals reproduced and fixed | 6 |
| CLI canonical copied name comparisons | 10 |
| DAP frame protocol tests | 66 |
| Scalar integer/IEEE-f32 property comparisons | 1020000 |
| Input hash checks | 117450 |

Each new fixture execution covers four native C++ narrow type families,
values -5 through 5, both branches, variable/cast and different-DIE cases,
C/C++/shared profiles and guest32/guest64 models. Native `decltype` witnesses,
selected-copy mismatch, conflicting same-DIE markers, unknown aliases, atomic
wrappers, invalid metadata, nested results, dead-arm non-evaluation and
promotion/name clearing are checked. Six executions each pass 60,293 checks.
The baseline compiles exact retained previous scalar/type headers in their
observed sibling include environment; it reproduces three old refusals per
repository. Five fixed CLI outputs per repository expose the canonical
`short` name. Previous generic computed-name behavior is evidenced by the
exact retained controller source, without claiming an old live name output.

Each repository emits 20 fresh C17/C23/C++20/Objective-C/Objective-C++ Wasm
modules across DWARF4/5 and O0/O2. The actual LLVM parser checks six narrow
base/alias functions per module: 48 C++ primitive bridge proofs and 72
C-family exclusions per repository. Alias display names remain intact.
Actual LLVM synthetic metadata additionally covers a misleading alias name,
an atomic wrapper hidden within a typedef, incorrect encoding and language
exclusion. The expression callback uses finite immutable/copied values;
these are metadata/data-path witnesses, not authenticated live guest reads.

PPC64 big-endian and x86_64 ELF each run ten components for both repositories.
Machine, width and byte order are checked, and each target output must equal
its pinned native component. Other architecture SDKs remain outside the
current recovered provider set. The real production controller frontend
compiles in both uwvm-int/JIT-off and LLVM/JIT-only macro families, retaining
ROS mode scope. No fresh full product link or debugger session is claimed.

No unsuccessful attempts occurred in this cut. Successful work summed to
1056.52 seconds across jobs; this is a sum of work,
not wall time or a performance result. DATA peak own RSS was
797257728 bytes (760.32 MiB)
under its unchanged 1 GiB limit. Controller frontend peak was
2195877888 bytes (2.05 GiB)
under the separate original 16 GiB compiler budget/9 GiB admission reserve.
Each class retains its 64 MiB own-output and 8 MiB per-file limits. Filesystem,
inode, CPU affinity and host disk admission floors remain. No OOM counter
increased. The guarded functional collector runs separately after these jobs.

## Retained evidence

Ordinary immutable source cut: `sha256:2d594f3d0c2601dc4cad9759f571111cb64f4165aa5d57a03ef1117168b4b53c`.
ROS immutable source cut: `sha256:3e45c638ae1872b1d0dcc6c78cfacb5acc09875e70fee325621750c2d4c64a5c`.
The nine owned implementation/fixture/registry paths equal the tested cuts
and match across repositories. The remaining source files are pinned in the
immutable manifests; later peer changes are not silently included.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/narrow-bridge-a1/narrow-bridge-evidence-a1.tar.xz`.
Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-narrow-bridge-evidence-20261007-a1/narrow-bridge-evidence-a1.tar.xz`.
Archive 32166040 bytes, 7147 regular members,
SHA-256 `de6091a07f5b3d4ac6532b653ed2d773cbfd07417897361bec276a198224fbee`. Both copies were fsynced and every member hash
verified. Sources, exact old headers, native/target binaries, Wasm modules,
actual dependencies, guard receipts and pinned recipes are retained. QEMU
binaries and the previous provider archive identity are included. Installed
LLVM remains an external provider recorded by binary/dependency hashes;
this archive does not contain the whole SDK. Final paired patches and
report/inventory updates are retained in a separate dual-verified sidecar.

After archiving, the hard 16 GiB image occupied
2747281408 physical bytes
(2.559 GiB).
Machine-readable evidence and remaining scope are in
`debug_narrow_bridge_qualification_20261007.json`.
