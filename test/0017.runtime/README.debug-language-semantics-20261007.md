# Selected-frame numeric language semantics — 2026-10-07

`uwvm2` and `uwvm2-ros` now carry the actual selected physical/inline
scope's compilation-unit language through DWARF indexing, stopped-frame
selection and both controller scalar evaluation paths. C/Objective-C numeric
conditional expressions promote classified narrow integers and Boolean
operands, including pairs with equal representation, to the guest signed
32-bit `int`. C++/Objective-C++ Boolean literals, comparisons, logical
operations and two Boolean conditional branches produce a copied 8-bit
Boolean result on the supported Wasm ABI.

| Expression at the corresponding selected source frame | C/Objective-C result | C++/Objective-C++ result |
|---|---|---|
| `true` | shared literal `1`, signed 32-bit | `true`, Boolean 8-bit |
| `1 < 2` | `1`, signed 32-bit | `true`, Boolean 8-bit |
| `0 && absent` | `0`, signed 32-bit; no absent read | `false`, Boolean 8-bit; no absent read |
| `1 ? (bool)255 : (bool)0` | `1`, signed 32-bit | `true`, Boolean 8-bit |
| `1 ? (signed char)-1 : (signed char)2` | `-1`, signed 32-bit | unavailable pending exact type/category proof |
| `0 ? (unsigned char)255 : (unsigned char)2` | `2`, signed 32-bit | unavailable pending exact type/category proof |

For example, at a genuine C++ source stop:

```text
print THREAD STOP 1 < 2
print THREAD STOP 1 ? (bool)255 : (bool)0
print THREAD STOP 0 && absent
```

At a genuine C source stop:

```text
print THREAD STOP 1 ? (signed char)-1 : (signed char)2
print THREAD STOP 0 ? (unsigned char)255 : (unsigned char)2
```

`THREAD` and `STOP` denote the current authenticated identifiers. Native/ASM
trap restrictions and existing frame, participant, generation, epoch and
incarnation checks still apply. The helper selects metadata only after the
controller authenticates the stopped activation/selected frame. It carries
no host pointer, location evaluator, memory capability or permission to
debug the VM. Both conditional arms supply type metadata, and only the
selected arm supplies copied values. Every selected copy must match its
inferred width, sign, floating state and scalar category. The controller
projects computed Boolean results as `bool` / `numeric_kind::boolean` in
both its copied-scalar and coherent guest-memory transaction paths.

C99 compilation units with exact producer `TinyGo` retain the existing
shared numeric dialect; they do not acquire C language rules merely from
`DW_AT_language`. Rust, standard Go, Zig, AssemblyScript and unknown units
also keep their separately documented limits. The exact `TinyGo` producer
recognition follows the existing indexer's route. A different producer label
is not a newly qualified TinyGo route.

Language comes from the selected scope's own CU. Missing language, an
invalid active scope path or declaration/origin links crossing a CU do not
select another frame's/CU's language. Cross-CU metadata remains parseable
when otherwise valid, with language unavailable and the existing conservative
shared dialect. Flag-form language attributes are rejected as malformed.
Reference graph traversal, producer text and CU counts use existing bounded
budgets. This step supports legacy `DW_AT_language`, not full DWARF6
`DW_AT_language_name`/version dialect handling.

C++ same narrow integer operands still need exact declared identity and
value category; equal bit width and sign cannot establish those properties.
This does not implement enum/pointer/class/overload results, glvalue/reference
identity, `char` rank distinctions, complete C/C++ parsing or full native
language debugger parity. C17's accepted `true`/`bool` spellings are debugger
shared syntax, not a claim that they are C17 source tokens. C23's standalone
`true`/`false` native Boolean type is also still unimplemented: the C profile
keeps the shared 32-bit literal convention even though C23's conditional
integer promotion is covered. Distinguishing C23 from older CU language codes
and complete C++ character-literal typing remain explicit follow-up gaps. The fixed
literal/predicate Boolean extent is the existing supported Wasm ABI; other
producer Boolean layouts remain unqualified. Objective-C dynamic dispatch,
Rust trait/collection evaluation, Go runtime collections/goroutines and
AssemblyScript typed source values remain separate gaps.

The conversion/result rules were checked against
[C23 conditional arithmetic rules](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf),
[C++ conditional rules](https://eel.is/c++draft/expr.cond),
[C++ logical result rules](https://eel.is/c++draft/expr.log.and) and
[LLVM's DWARF language registry](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/BinaryFormat/Dwarf.def).

## Qualification

All builds, target execution and functional verifiers ran on SSH Linux in
the reused 64 GiB, swap-0 cgroup, behind a birth/PIDFD-bound owned-process
supervisor. Boot: `d9ee997c-9129-43ea-a0f3-c9785344bba8`; keeper PID/birth:
`9769/16929`. The shared bounded ext4 arena remains
16 GiB, with filesystem ID `12191019222208619510`. No foreign process,
source modification or prior evidence was removed.

| Check | Result |
|---|---:|
| Successful guarded jobs | 10 |
| Native scalar/compiler/protocol phases | 50 |
| Actual LLVM metadata/compiler-producer phases | 48 |
| Actual controller frontend mode paths | 4 |
| Fresh embedded-DWARF Wasm producer modules | 40 |
| LLVM index/producer assertions | 1868 |
| Numeric language/profile/frame assertions | 65802 |
| Actual QEMU components | 32 |
| Compiled C17/C23 `_Generic` type assertions | 20 |
| C17/C23 runtime value comparisons | 3240 |
| Original old-header conditional refusals reproduced | 8 |
| Original C++ Boolean width/category mismatches reproduced | 8 |
| DAP frame protocol tests | 66 |
| DAP corpus per native job | 263 positive / 223 negative |
| Scalar integer/IEEE-f32 property checks | 1020000 |
| Boolean regression assertions | 7771902 |
| Zig regression assertions | 57912 |
| Input hash checks | 104958 |

Each native/QEMU language fixture checks 10967
assertions over Wasm32/Wasm64 type models, C/C++/shared profiles, five same
narrow type families, selected-reference-only reads, nested Boolean
conditionals, comparison/logical operators, unclassified/wide Boolean
refusal, invalid profile and selected-inline/physical metadata paths.
Compiler-native C++ `decltype` assertions and C17/C23 `_Generic` witnesses
establish the differing native result types. Legacy API probes are built
against the exact retained old scalar header, with actual dependencies
recorded, before being compared with the new C/C++ profiles.

Each repository compiles twenty fresh Wasm modules: C17, C23, C++20,
Objective-C and Objective-C++, crossed with DWARF 4/5 and O0/O2. The real
LLVM index parses embedded sections, obtains concrete source scope ranges,
classifies the CU and drives conditional result widths. Actual LLVM synthetic
fixtures additionally test TinyGo exclusion, wrong attribute form and
cross-CU origin refusal. These are metadata tests; they do not supply a
runtime pause ticket or stopped guest read. No new TinyGo compiler/live
session was tested in this cut.

The actual production controller is compiled with explicit uwvm-int/JIT-off
and LLVM/JIT-only flags in both repositories. These frontend checks cover the
new helper and both macro families, with the ROS mode scope unchanged. They
emit no product executable and do not qualify a link, runtime handshake,
interactive `-Rdbg` stop or source expression output at a real stop.

QEMU runs PPC64 big endian and x86_64 target ELF for both repositories.
Machine, width and byte order are verified; each target runs eight
components and has byte-identical output to its pinned native reference.
The additional architectures' SDK providers remain outside this recovery
scope. Complete language functionality and all-architecture product parity
remain unqualified.

Three unsuccessful harness attempts are retained: an old-header Boolean
baseline expectation omitted the original two-Boolean refusal; the Wasm
producer recipe initially used `--ld-path`, ignored by system Clang for that
target, before switching to the pinned SDK `-B` directory and `-fuse-ld=lld`;
and the controller interpreter frontend initially omitted explicit mode
macros, before selecting uwvm-int/JIT-off. Production sources were unchanged
through these corrections. Successful tests were rerun from the same
immutable source cuts with revised, pinned recipes.

Successful guarded jobs took 899.93 seconds in
aggregate; all retained attempts took 1075.67 seconds.
These totals sum work and are not elapsed wall-clock or performance results.
Peak owned DATA RSS: 793952256 bytes
(757.17 MiB), within 1 GiB.
Peak controller frontend RSS: 2195701760 bytes
(2.04 GiB), within its separate
original 16 GiB compiler budget/9 GiB shared-memory admission reserve.
The 1 GiB DATA guard was not relaxed. Each class retains a strict 64 MiB
owned-output cap and 8 MiB individual file limit. The frontend uses the
original full-compile filesystem floor, while DATA retains a 1 GiB floor;
both retain 4096 inode and 25 GiB host-disk reserves and owned-tree retirement.
OOM counters did not increase.

## Evidence

Ordinary source cut: `sha256:5786e45dd868d8597c029e51de84fc452821e805fd925e789c64120ffbf601d2`.
ROS source cut: `sha256:8b45d28de62e6b2c2c0f37cdaf2d52d9dcb7bd697dd7bee5fabeaf6058c06747`.
The eleven owned implementation/fixture/registry paths match both tested
cuts and both repositories byte for byte. Peer changes in other paths remain
outside the owned patch. Earlier full-product qualification keeps its own
source identity and is not promoted to this new cut.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/language-semantics-a1/language-semantics-evidence-a1.tar.xz`.
Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-language-semantics-evidence-20261007-a1/language-semantics-evidence-a1.tar.xz`.
Archive: 32470768 bytes, 7288 regular members,
SHA-256 `627e79d9bf9ecf73908ace882bcd687d234cb29da369f867da1be4925d5394b1`. Both copies were fsynced and every member hash
verified. They retain source cuts, old headers, native/target component
binaries, fresh Wasm files, actual compiler dependencies, recipes and failed
and successful receipts. QEMU binaries and the previous provider archive
reference are retained. The installed LLVM SDK is an external provider;
exact observed binary/dependency hashes are recorded, and this archive does
not claim to include that SDK in full.

The 16 GiB image occupied 2304995328 physical
bytes (2.147 GiB) after
archiving. See `debug_language_semantics_qualification_20261007.json` for
machine-readable details and explicit remaining gaps.
