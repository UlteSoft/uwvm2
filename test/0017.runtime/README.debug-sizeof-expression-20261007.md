# Unevaluated compound sizeof — 2026-10-07

Both uwvm2 and uwvm2-ros now accept `sizeof` of the already supported finite
fixed-size scalar expression grammar. Operand types are inferred from literals,
builtin casts and the selected frame's immutable declaration metadata.
The compound operand is never evaluated and invokes no value/sizeof resolver.
Unavailable variable values and division/shift errors do not prevent a size
when the declaration and expression type are available.

| Example | Size on the supported Wasm32 ABI |
|---|---:|
| `sizeof(a + b)` with declared int operands | 4 |
| `sizeof(a / 0)` with declared int a | 4 |
| `sizeof(1 << -1)` | 4 |
| `sizeof((short)a)` | 2 |
| `sizeof(real32 + real64)` with float/double declarations | 8 |
| `sizeof('(' + ')')` | 4 |
| `sizeof(sizeof(packet) + 1)` | 4 |
| `sizeof(1 ? short_a : short_b)` | C++: 2; C/C23: 4 |
| `sizeof(true)` | C++/explicit C23: 1; finite C/shared profile: 4 |

Use the actual thread and stop identifiers of an authenticated source pause:

```text
print THREAD STOP sizeof(a + b)
print THREAD STOP sizeof(a / 0)
print THREAD STOP sizeof((short)a)
print THREAD STOP sizeof(real32 + real64)
```

`a`, `b` and other operands must be declared in the selected source frame with
supported type metadata. Missing declarations still make the result unavailable,
even for a short-circuited operand such as `sizeof(1 || missing)`.
The existing DAP watch/hover/evaluate route accepts the same syntax and retains
its original frame/stop envelope. These examples describe the implemented
interface; this cut does not link a fresh full product or verify live `-Rdbg`
commands. Production controller frontends and protocol DATA are qualified below.

## Implementation

The parser first preserves builtin type and direct object-selector `sizeof`
handling. Other operands become owned `expression_size` syntax nodes sharing
the existing parser, 32-level recursion, 128-node and 4096-byte scalar limits.
The DAP/console request envelope remains limited to 256 bytes. One-character
literals and their supported escapes hide their own parentheses during
delimiter scanning; numeric digit separators keep their existing syntax.
Boolean literals are interpreted as scalar syntax rather than variable names.

Evaluation of the new node calls only the bounded existing type-inference
routine. That routine checks both arithmetic/conditional operand types, accepted
scalar categories, actual guest widths and 4096-visit budget. It does not run
arithmetic or invoke a value reader. The result extent is the unpromoted outer
operand type's width divided by eight, so a short cast has size 2 while a unary
plus on short has the promoted int size. Arithmetic and conditional common
types retain the prior C/C++ rank semantics. Nested sizeof remains type DATA.
A missing type resolver never falls back to the value callback.

The result retains a generic unsigned guest-sized carrier. No operand DIE,
native pointer, writable glvalue, read token or guessed size_t identity escapes.
Both existing production controller callbacks select type metadata from the
same authenticated source frame. The direct aggregate/pointer/object route
keeps its existing extent callback and bit-field checks; the no-value-callback
proof here applies to the new compound path. No controller transaction or ASM
domain authority was widened; ASM remains confined to generated Wasm contexts.

C++ test printing and string construction use fast_io; dynamic operation names
use bounded string_view. Existing numeric scanning uses fast_io parse_by_scan.
The C witness has no IO.

The behavior was checked against [C++ sizeof](https://eel.is/c++draft/expr.sizeof)
and [C23 6.5.3.4](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf).
The implemented subset has fixed scalar extents. C variable-length array
exceptions, user-defined type names, incomplete/function types, calls,
assignments/increments, overloads, complete bit-field/reference/glvalue rules,
wide/string literal expressions and complete native expression grammar remain
unfinished. `sizeof(call())`, `sizeof(++a)` and computed-pointer syntax remain
unsupported, including within an unevaluated operand.

Explicit C23 CU selection supports its Boolean constants; fresh C23 samples
still carry older CU language codes, so automatic dialect/version selection
remains unfinished. Rust, standard Go, TinyGo, Zig and AssemblyScript native
intrinsics/evaluators are not newly qualified by this shared syntax extension.
Exact size_t identity, full ptype/CV/typedef/reference behavior, fresh full
product/live sessions and remaining architecture product parity remain open.

## Verification

All compiler, executable and functional Python checks ran on SSH Linux in the
existing 64 GiB/swap-0 cgroup, boot `d9ee997c-9129-43ea-a0f3-c9785344bba8`, keeper PID/birth
`9769/16929`. Birth/PIDFD supervision retires only
owned children. Administrative capture, archive/hash checks and report edits
ran no local functional tests.

| Current tested cut | Passed |
|---|---:|
| Guarded native/metadata/frontend/QEMU jobs | 12 |
| Native compiler/scalar/protocol phases | 80 |
| Actual LLVM metadata and producer compiler phases | 48 |
| Production controller frontend mode paths | 4 |
| Fresh DWARF Wasm32 modules | 40 |
| Actual metadata-driven unevaluated sizeof proofs | 840 |
| Independent Wasm sizeof compiler witnesses | 480 |
| Independent C17/C23 native sizeof compiler witnesses | 56 |
| Native C runtime sizeof checks | 8 |
| New sizeof assertions with value-callback spies | 50600 |
| Actual parser/producer assertions | 8040 |
| Standard-rank regression assertions | 855880 |
| Primitive-name bridge regression assertions | 482344 |
| Exact narrow/C23 regression assertions | 140984 |
| QEMU components equal to pinned native stdout | 72 |
| Exact old parser refusals reproduced and fixed | 12 |
| DAP frame/protocol tests | 66 |
| DAP/controller corpus positive/negative expressions | 279 / 231 |
| Existing scalar/IEEE property comparisons | 1360000 |
| Input hash checks | 151108 |
| Additional current peer-updated DAP protocol tests | 148 |

Each new fixture execution passes 6325 checks. Native C++ sizeof/decltype
witnesses cover all 36 pairs of six standard integer families, arithmetic,
bitwise operations, conditional and shifts. Guest32/64 and explicit C/C++/C23
profiles cover casts, predicates, character/Boolean literals, nested sizes,
metadata-only dereference/member types, missing/invalid declarations,
unavailable values, division/shift/overflow non-evaluation, absent type callbacks,
exact node limits and hostile type-graph cycles. Shared ambiguous narrow/Boolean
conditional refusals remain. All successful compound size probes observe zero
value/sizeof callback calls.

Each repository compiles 20 fresh C17/C23/C++20/Objective-C/Objective-C++ modules
across DWARF4/5 and O0/O2, with twelve sizeof static assertions per module.
Actual LLVM parsing checks three size expressions against each of seven real
wide declarations in every module, producing 420 metadata proofs per repository.
These copied declaration callbacks do not authenticate a live runtime stop.
Fresh metadata uses Wasm32; guest64 qualification here is finite numeric DATA.

PPC64 big-endian, x86_64 and aarch64 each execute twelve current-cut components
in both repositories under QEMU. Target ELF machine/width/byte order, actual
dependencies and stdout parity are checked. This is component qualification,
not full-product/native-hardware/all-architecture equivalence. ROS's full
uwvm-int/full LLVM-JIT mode scope was not edited.

Before closure, a concurrent agent synchronized a WASIp1 edit-acknowledgement
fix into both DAP adapters. These peer changes were preserved and separately
captured. All nine other implementation paths remain byte-identical to the
original tested sizeof cut. Both current adapters then passed 33 source-frame,
28 WASIp1 state and 13 commit-observation protocol DATA tests per repository,
and the unchanged 279/231 expression corpus. These two additional guarded jobs
are separate from the twelve original-cut jobs and do not establish live VM or
native acknowledgement interoperability. Their original source, recipes and
receipts are retained under adapter-followup-a1; no ownership of the peer fix
is claimed here. Current adapter SHA-256: `1d06d5d6c8d6c9d514edbc4e06695403c5e21c44af3fec1e23b2dff13ee3c7f4`.
Followup DATA own RSS peaked at 76177408
bytes; its guard retirement and unchanged zero OOM counters were verified.

## Attempts and resource control

Two unsuccessful test attempts are retained: the A1 fixture used an unwrapped
dynamic C string in fast_io concat; A2's verifier assumed unsupported status 3
although the retained original header reports 2. The fixture and expectation
were corrected, and both repositories completed their native checks in a new
owned role. The A2 implementation source did not change during the verifier
retry. Earlier metadata/frontend passes and both failed receipts/logs/source
identities remain separate from the current-cut counts above.

The first final collector failed while reading an absent optional error field
from a successful receipt. The collector was corrected to use an optional-field
lookup and passed in a new guard directory; original failed collector source,
receipt and log remain. This verification-only failure did not change the
implementation or require repeating passed compiler/executable tests.

Administrative staging also encountered local ENOSPC, a previous/current stream
proof filename collision and an x86 queue-label underscore refusal before
worker admission. Failed partial uploads and startup records were retained.
Exact-byte checks resumed the unexecuted extraction; x86 used a new command
and guard directory. No test/compiler launched during those staging/startup
failures. Native compilation/test results and functional Python verification
still ran exclusively inside the cgroup.

Successful current-cut guarded work totaled 1626.01
seconds; all retained job attempts totaled 1990.15 seconds.
These sums include retained failed attempts and the failed verification-only
collector, and exclude the final successful collector and administrative work.
They are sums of job work, not wall time/performance. OOM counters did not
increase. Peak DATA own RSS was 811425792 bytes, below
its original 1 GiB per-job limit; frontend peak was
2195095552 bytes, below the separate 16 GiB compiler
limit. Native, cross and metadata roles retain independent original 64 MiB
own-output/8 MiB per-file bounds; concurrent QEMU guards enforce the same
cross-role output bound. No quota was relaxed.

All outputs use the same hard 16 GiB ext4 arena with original inode/disk reserves.
The image occupied 3694510080 physical bytes
(3.441 GiB) after archiving.
Source upload streamed directly to Linux. Ten prior task-owned large local
archive duplicates were retired only after exact Linux/local hashes and sizes
matched; fsynced Linux archives, raw test data and local small reports/patches
remain. The two retirement records account for 404020248 bytes (385.3 MiB)
reclaimed locally. No foreign cleanup occurred.

## Evidence

Ordinary source cut: `sha256:ffbba9035cd7a142f8bf21347db056e9ed04830dab134605ff493e3486f51f3d`.
ROS source cut: `sha256:fe581d89fbd8b0b65a840db0ca6d9c53a2ec9054b41bd903857a7280cff44897`.
The ten implementation/fixture/adapter/registry paths match their tested cuts
and both repositories: nine match the original cut, the adapter matches its
separately verified followup cut. The inventory preserves all old top-level fields and
other features; sizeof_type remains partial.

Large evidence archive retained on Linux: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/sizeof-expression-a2/sizeof-expression-evidence-a2.tar.xz`.
Archive 56671112 bytes; 7813 regular members;
SHA-256 `1030eedd7288fa20df7bdfe904b67946dfa4f2debf6202a33e1f4748e798bc11`. Every member was verified on Linux and the archive
fsynced. Earlier source cuts reconstruct from retained changed bytes and current
identical members. Original earlier Linux trees also remain. LLVM/cross SDK
providers remain external with pinned identities and previous archived provenance.
This is not a self-contained SDK or fresh full-product qualification.

Local qualification/proof summaries match retained archive members; no large
local archive copy is claimed. Final paired patch/source/documents are retained
in a separate small, member-verified and fsynced archive on both hosts.
See `debug_sizeof_expression_qualification_20261007.json` for exact records
and the unfinished scope.
