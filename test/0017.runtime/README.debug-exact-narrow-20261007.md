# Exact narrow copied results and C23 Boolean constants — 2026-10-07

Both `uwvm2` and `uwvm2-ros` retain narrow C++/Objective-C++ conditional
results when immutable type DATA proves the same integer type. Explicit
standard builtin casts retain separate identities for `char`, `signed char`,
`unsigned char`, `short` and `unsigned short`. Declared variables can use the
exact canonical DWARF type DIE key, including its compilation-unit identity.
C23 has a separate profile selected only by explicit `DW_LANG_C23` (`0x3e`).
Its predefined `true`/`false` produce a copied Boolean value on the supported
one-byte Wasm ABI; its arithmetic, comparison, logical and Boolean
conditional results continue to use C integer promotion.

| Expression | Selected C/C23 frame | Selected C++/Objective-C++ frame |
|---|---|---|
| `1 ? (signed char)-1 : (signed char)2` | signed 32-bit `-1` | signed 8-bit `-1` |
| `0 ? (unsigned short)1 : (unsigned short)65535` | signed 32-bit `65535` | unsigned 16-bit `65535` |
| `1 ? (char)1 : (signed char)2` | signed 32-bit `1` | signed 32-bit `1`; these are distinct types |
| `1 ? left : right`, both declared with the same canonical narrow DIE | signed 32-bit promotion | original narrow integer representation |
| `1 ? (i8)1 : (i8)2` | classified numeric alias promotion | unavailable: a shared alias spelling does not prove exact C++ type |
| `'a'` | signed 32-bit `97` | ordinary ASCII `char`, 8-bit, on the default signed-char Wasm ABI |
| `true` / `false` | C23: Boolean 8-bit; older C profile: shared signed 32-bit literal | Boolean 8-bit |
| `!false` / `true < false` | signed 32-bit `1` / `0` | Boolean 8-bit |
| `1 ? true : false` | signed 32-bit `1` | Boolean 8-bit |

Example commands at an existing authenticated source stop:

```text
print THREAD STOP 1 ? (signed char)-1 : (signed char)2
print THREAD STOP 0 ? (unsigned short)1 : (unsigned short)65535
print THREAD STOP 1 ? left : right
print THREAD STOP 1 ? 'a' : 'b'
```

`THREAD` and `STOP` are the actual current pause identifiers. The examples
require the corresponding real selected source frame and language metadata;
`left` and `right` must be supported numeric variables of the same canonical
narrow type. These commands produce read-only scalar copies. This step does
not return a writable C++ lvalue or implement reference identity/assignment.

## Type evidence and boundaries

Both controller scalar paths attach canonical type identity from the actual
selected declaration or immutable leaf type layout. The coherent
memory-transaction path retains the existing authentication/transaction,
and the copied-local path retains its existing no-memory-reader authority.
An extra type-only query for a root narrow copied value reads no guest bytes.
Missing extra identity leaves the existing numeric value path available;
exact C++ narrowing still fails when sufficient type evidence is absent.
The type helper rejects non-scalar/atomic types and extent mismatches.
No name-only type guessing, native address, host read or VM debug permission
is introduced. Existing ASM generated-Wasm-context restrictions remain.

Conditional inference queries both operand types before reading either
operand value. Only the selected arm is evaluated. The selected copy must
match width, sign, floating state, scalar category and the inferred narrow
identity. An identity mismatch returns unavailable and clears the result.
Zero/absent canonical keys, another CU, or independent duplicate type DIEs
cannot establish the same type merely because size/sign/name agree.
Canonical aliases already resolved to the exact same DIE may share that
identity; builtin-versus-DIE compatibility and separately emitted duplicate
DIE compatibility remain unimplemented. This deliberately preserves refusal
where equal representation alone does not establish native type identity.

Builtin identities normalize valid standard specifier order, keeping `char`
distinct from `signed char`. Shared aliases such as `i8`/`i16` retain their
existing finite numeric syntax but do not acquire native C++ type identity.
Integer promotion clears narrow identity; nested narrow conditionals retain
it only when proven. The default three-argument/shared numeric API remains
conservative and does not automatically select a source language.

The finite ordinary character literal subset is a single ASCII character or
an existing supported ASCII escape. It returns `char` in the C++ profile,
`int` in C/C23. It does not qualify `-funsigned-char`, non-ASCII, multicharacter,
wide/UTF-prefixed literals or a producer-specific character ABI. Exact
computed `ptype` spelling/CV/rank display remains partial: the controller
currently projects integer category, byte extent and value, and its generic
computed integer name is not full native type reconstruction.

C23 selection requires explicit CU language code `0x3e`. The fresh system
Clang C23 modules in this run emitted CU language codes
`[29]`,
which classify as the older C profile. Their native `_Generic` witnesses
prove C23's language rules; they do not qualify automatic C23 version
selection in a live session from those old CU codes. Actual LLVM-parsed
synthetic `DW_LANG_C23` units exercise the new selection route. Producer
strings and compile command labels are not language-version evidence.
Full DWARF6 language/version handling remains a gap.

TinyGo exclusion and the separate shared numeric dialect for Rust, standard
Go, Zig, AssemblyScript and unknown producers remain as documented in prior
qualifications. This cut contains no fresh TinyGo/Go/Rust/Zig/AssemblyScript
compiler or full stopped-language session qualification. Native collections,
traits, goroutines, dynamic Objective-C dispatch, enum/pointer/class/overload
conditional rules, C++ glvalues/references/CV and complete language parsing
remain separate unfinished items.

The language rules were compared with
[C23 predefined constants and conditional arithmetic](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf)
[C++ conditional result typing](https://eel.is/c++draft/expr.cond) and
[C++ character literal types](https://eel.is/c++draft/lex.ccon).
The registry value is recorded in
[LLVM's DWARF language definitions](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/BinaryFormat/Dwarf.def).

## Verification

Every build, target run and functional verifier ran on SSH Linux inside the
reused 64 GiB/swap-0 cgroup. Boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`, keeper PID/birth
`9769/16929`. A birth/PIDFD-bound supervisor admits
and retires only its own process tree. The shared cgroup and bounded 16 GiB
ext4 arena were reused; no peer source edits, foreign processes or prior
evidence were removed.

| Check | Passed result |
|---|---:|
| Guarded build/test jobs | 10 |
| Native compiler/scalar/protocol phases | 64 |
| Real LLVM metadata and Wasm compiler phases | 48 |
| Actual controller frontend checks, uwvm-int and LLVM modes | 4 |
| Fresh embedded-DWARF Wasm modules | 40 |
| Actual parser/producer assertions | 1936 |
| Real canonical C++/Objective-C++ narrow DIE proofs | 16 |
| Exact narrow/C23 positive and negative assertions | 105738 |
| Numeric language regression assertions | 65796 |
| Actual QEMU components | 36 |
| Independent C17/C23 compiler type assertions | 44 |
| Independent C17/C23 runtime value checks | 3252 |
| Original refusals reproduced and fixed | 8 |
| Original narrow/C23 type mismatches reproduced and fixed | 8 |
| DAP frame protocol tests | 66 |
| Scalar integer/IEEE-f32 property comparisons | 1020000 |
| Input hash checks | 111222 |

The new fixture compares four native C++ narrow type families over values
-5 through 5, both selected branches, C/C++/C23 profiles and guest32/guest64
models. It also tests nested conditionals, standard specifier permutations,
`char` versus `signed char`, no dead-arm arithmetic evaluation, canonical
identity mismatch/absence/cross-CU refusal before value reads, selected-copy
identity mismatch and exact C23 Boolean versus C integer result categories.
Native C++ `decltype` and independent C17/C23 `_Generic` assertions provide
compiler type witnesses. The old probe compiles the exact retained previous
scalar header with observed sibling headers and records its real dependencies.

Each repository builds 20 fresh C17/C23/C++20/Objective-C/Objective-C++ Wasm
modules across DWARF4/5 and O0/O2. The actual LLVM parser reads their embedded
metadata. Eight C++/Objective-C++ modules per repository prove that both
volatile narrow variables resolve to the same canonical base DIE. The
numeric expression fixture then uses that immutable metadata with a
selected-only finite value callback. It is a metadata/data-path witness,
not an authenticated live guest pause/read.

PPC64 big-endian and x86_64 target ELF each run nine components in both
repositories, checking ELF machine/width/byte order and byte-identical output
to the pinned x86_64 native component reference. Additional architectures'
SDKs remain outside this recovered provider set. The real production
controller frontend is compiled in both uwvm-int/JIT-off and LLVM/JIT-only
macro families; these checks do not link a product or qualify a fresh
interactive `-Rdbg` handshake/stop/expression result. Earlier full-product
runs retain their original source identity and are not promoted to this cut.

Two unsuccessful startup attempts are retained. The first native and
frontend launches omitted the verified CPU-list environment variable and
were refused by the cgroup prerequisite before any compiler ran. The empty
output directories were preserved under `failed-admission-*`; corrected
launches used the same source cuts and pinned recipes. No source change was
made to bypass the admission check.

Successful work totaled 1003.09 seconds across
all jobs; retained attempts totaled 1005.25 seconds.
These are sums of work, not elapsed/performance results. DATA's peak own RSS
was 796794880 bytes
(759.88 MiB), under its unchanged 1 GiB
limit. Controller frontend's peak was 2195828736
bytes (2.05 GiB), under the separate
original 16 GiB compiler budget/9 GiB admission reserve. Each class retains a
64 MiB own-output limit and 8 MiB per-file limit; DATA's 1 GiB filesystem floor,
frontend's original full-compile floor, 4096 inode reserve and 25 GiB host
reserve remain. OOM counters did not increase.

## Retained evidence

Ordinary immutable cut: `sha256:c03fb277e39177011b61ae1cac65156935cd07509cf1e27d7800a0bf915b0ca2`.
ROS immutable cut: `sha256:1aa461db444ffd2b1e0cc39c8abe09106d5b8a4b116c70cd40379ddc508a41cc`.
The eight owned implementation/fixture/registry paths equal their tested
cuts and match across repositories. ROS mode scope is unchanged.

Linux archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/exact-narrow-a1/exact-narrow-evidence-a1.tar.xz`.
Local archive: `/Users/liyinan/.codex/artifacts/uwvm2-exact-narrow-evidence-20261007-a1/exact-narrow-evidence-a1.tar.xz`.
Archive size 31873732 bytes, 7120 regular members,
SHA-256 `6b652cc5ecdeaa58481227fe478282944024a38492c7c5abc07edf55ba712a02`. Both copies were fsynced and every member hash was
verified. They retain immutable sources, exact old headers, native/target
binaries, Wasm files, actual dependency hashes, pinned recipes and failed/
successful guard receipts. QEMU binaries and the earlier SDK provider archive
reference are included. Installed LLVM is an external provider with observed
binary/dependency hashes; the archive does not include the entire SDK.

After archiving, the hard 16 GiB image occupied
2590498816 physical bytes
(2.413 GiB). Machine-readable
results and unfinished scope are in
`debug_exact_narrow_qualification_20261007.json`.
