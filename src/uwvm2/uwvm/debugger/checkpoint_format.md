# UWVM checkpoint state database format 4

This document specifies the portable state/database and replay component contracts.
It is not evidence that whole-VM save, restore or reverse execution has passed.
The current LLVM full compiler has observation safepoints, not complete portable
locals/operand/exception continuations. The runtime integration below must be
implemented and qualified before the controller advertises those capabilities.

Only LLVM JIT full may enable checkpoint instrumentation. Both repositories use
the same schema. Ordinary full, lazy, interpreter and tiered code must execute no
new checkpoint guards, shadow-stack writes, scheduling polls or I/O. A request in
an incompatible mode uses the existing colored `fast_io` fatal mode diagnostic.
A compatible mode lacking the actual materialization/import adapter capability
reports an explicit unavailable capability; it must not produce an incomplete
checkpoint labelled complete.

## Semantic boundary and identity

The Core 3 store contains functions, tables, memories, globals, tags, data/element
segments, structures, arrays and exceptions. References are abstract addresses
with identity; module-local type/function indices are different from store
addresses. Host functions are allowed to behave nondeterministically. These are
the basis for the schema, rather than native C++ object layout. See the
[normative runtime structures](https://webassembly.github.io/spec/core/exec/runtime.html).

A capture owns a closed admitted population of guest threads and a real current
execution-generation lease. The controller's private pause ticket proves every
actual participant is parked. GC store/cohort pins and native-root owners remain
live while constructing the graph. An OS TID, a copied stop number, a recorded
epoch, a GPR array, a DWARF value, or a native PC/SP does not grant this authority.
No pointer, GC token, registry control block, native exception handle, file
descriptor, HANDLE, mutex, guard page address, executable code or OS thread ID is
serialized. Restore allocates new objects and relocates references by logical ID.

Object IDs are dense unsigned 64-bit indices plus one in the complete object
vector; zero is invalid in object links. Reference null is encoded separately.
The single namespace makes aliases and GC cycles unambiguous. Module type keys
are `(module ID, u32 type index)`, retaining recursive/subtype identity from the
exact embedded module. A restore must use the same fused Core 3 validator and
compiler to validate that module and every field, tag, signature, subtype, control
edge and current function body. `validate_graph` is bounded structural validation
and does not replace semantic module validation.

Version 4 additionally has a TYPE-ONLY empty-table descriptor. A table of
logical length zero has no element/default runtime value, even when its declared
reference type is nonnullable. It cannot manufacture null or retain a reference
from the initializer; capture must not rerun the initializer. Its declared type
is carried by one uninitialized canonical zero cell, scoped only to that empty
table descriptor. Version 3 represented uninitialized nondefaultable frame locals.
Versions 1 through 3 are not accepted by the current state decoder; no migration
is inferred from matching object IDs, a checksum or native pointer-shaped data.
Version 2 separated Core 3 host references from external wrappers. Version 1
conflated those states and is an immutable development artifact, not accepted by
the current codec or converted by guessing an adapter. An explicit future
migration would need actual runtime type/adapter proof.

Core 3 types bare `ref.host` as any and types `ref.extern(inner)` as extern only
when inner is a nonnull any reference. The format therefore represents host
identity and wrapping separately. `extern.convert_any` of a struct/array/host
keeps its existing object ID as the wrapper's single inner value; i31 keeps its
31 bits. `any.convert_extern` recovers that same inner identity. Null remains
plain null. Wrapper nesting cannot be valid because extern is outside the any
hierarchy; validation inspects one bounded value and never recursively follows
wrapper IDs. A mutable struct containing an extern field that wraps that same
struct remains a legitimate GC cycle. Runtime semantic validation additionally
checks all actual defined types and rejects immutable-only object cycles as
required by store validity. See [reference value typing](https://webassembly.github.io/spec/core/exec/values.html#valid-ref),
[conversion execution](https://webassembly.github.io/spec/core/exec/instructions.html#syntax-instr-extern)
and [store validity](https://webassembly.github.io/spec/core/appendix/properties.html#valid-store).

## Binary encoding

All integers use `fast_io::mnp::le_put` / bounded `parse_by_scan(le_get)`.
No struct memcpy or host-endian integer representation enters the file. Float
values preserve their original bits, including signed zero and NaN payloads.
v128 low/high words preserve Wasm lane byte order. i8/i16 are packed storage
values, never general locals or globals. i31 is an immediate unsigned 31-bit
payload and does not allocate a logical object.

State6 uses distinct magic `UWCPST6\0` and commit marker `UWCPSTE6`.
The outer identity envelope keeps `UWCPDB4\0` / `UWCPEND4`, version4 and its
160-byte header; inner and outer documents are never selected by guessing header
lengths. State schema6 is also the third field of the actual per-engine checkpoint
profile/cache tuple. Binding identity revision2 stays unchanged; the native continuation ABI is
independently recorded by the actual compiler profile and binary build; old schema5/4/3 profile/cache contexts cannot admit schema6 state.

Header length is 128 bytes. In order:

| Field | Encoding |
| --- | --- |
| magic | 8 bytes `UWCPST6\0` |
| format version, header bytes | u16 6, u16 128 |
| endian sentinel | u32 `0x04030201`, bytes `01 02 03 04` |
| flags | u64 zero, reserved |
| recording identifier | 16 opaque nonzero bytes, not authentication |
| checkpoint ID, parent checkpoint ID | u64, ID nonzero and parent less than ID |
| logical instruction, replay event cursor | u64, instruction boundary and event count |
| required features, next logical thread ID | u64, compatibility requirements |
| object count, root instance count, retained value count | u64 bounded counts |
| body byte count, reserved | u64 exact count, u64 zero |

The body starts with root instance IDs (strictly ascending u64), then retained
typed values, then all object records in dense ID order. Each object starts with
u16 kind, u16 flags, u32 reserved zero, eight u64 words, and u64 link/value/byte
counts: 96 fixed bytes. Links are u64 IDs, followed by typed value cells, followed
by raw bytes. Every count is checked against both remaining bytes and resource
budgets before allocation or cursor movement. Unused words are zero.

A typed value occupies 48 bytes: u8 numeric/reference kind, u8 heap kind,
u8 nullable boolean, u8 reference kind, u32 value flags, u64 type module, u32 type index,
u32 zero, u64 reference target, u64 low bits, u64 high bits. Nonreferences have no
heap/module/type/reference target, and unused high bits are zero. Builtin heaps
are func, extern, any, eq, i31, struct, array, exn, nofunc, noextern, none and
noexn; a defined heap uses its module/type key. Non-null reference target kind
must agree with function/extern-wrapper/struct/array/exn/host; i31 has target zero. A bare host reference has heap any; an external wrapper has heap extern. Nullability
and builtin categories are checked structurally; defined subtype matching is
checked against the exact validated module on restore.

Value flags are zero for initialized values or one for a type-only/uninitialized
cell; other bits fail. Flag one is legal in exactly two contexts: among a frame's
first `local count` values for a declared nonnullable reference, or as the sole
typed descriptor of a table whose logical length is exactly zero. An empty table
MUST use that type-only descriptor, for nullable and nonnullable reference types;
a nonempty table MUST have an initialized actual default value. The exact declared
type remains present; reference kind/target/low/high are all zero without implying
a null value or retained GC root. Frame operands, globals, GC fields, actual table
elements, segments, retained roots, controls, exceptions and replay values cannot
be uninitialized. This descriptor grants no module/source/stop/restore authority. A real compiler
checkpoint must record actual executed local initialization and skip every load
from an unset alloca; the conservative source-display availability packet cannot
substitute for the complete runtime state. Versions 1 through 3 are preserved as
immutable development artifacts and are not migrated by guessing missing state.

The footer occupies 48 bytes: magic `UWCPSTE6`, u64 body count, and SHA-256 of the
exact header plus body using `fast_io::sha256_context`. Missing/footer mismatch,
extra bytes, corruption and reserved fields fail. SHA-256 detects corruption; it
is not management authentication or proof of mmap immutability. Decode builds
a detached graph and only replaces its caller's output after every check passes.

Feature bits 0 through 17 are, respectively: multivalue, reference types, SIMD,
relaxed SIMD, bulk memory, mutable globals, nontrapping float conversion, sign
extension, tail calls, memory64, multiple memories, table64, GC, function
references, exceptions, extended constants, threads and exception references.
This file compatibility mask is not a new command-line feature hierarchy.
Reserved bits fail; runtime feature controls still validate exact original code.

## Object records

The words listed below are followed by zeros through word 7. Links are in the
listed order, not sorted when their order is semantically significant.

| Kind | Flags / words | Links / values / bytes |
| --- | --- | --- |
| module | flags 0 legacy detached DATA, all words zero; flags 1 actual resolved syntax policy: revision 1, CLI mode 0..4, nineteen disable bits, two controllable bits | Original validated Wasm bytes, including recursive type groups, all declarations, custom/debug sections and immutable initialization metadata. No links or values. |
| instance | flags 0; counts of functions, tables, memories, globals, tags, data, elements | Module ID followed by those seven ordered index-to-store maps. Imported aliases use the same object IDs. No values/bytes. |
| function | flags 0 Wasm / 1 host; local function index, body generation, type index | Instance ID; host additionally has adapter/resource ID. Original Wasm body uses the module; a replacement has its exact validated local-declaration/body bytes. Never native code. |
| memory | flags 0 private / 1 shared; address bits 32/64, current pages, minimum, maximum | Byte pages are represented by memory chunks, omitted bytes explicitly zero. No pointers or mmap layout. |
| memory chunk | flags 0; unsigned byte offset | One memory ID; nonempty bytes. Chunks for each parent are ascending/nonoverlapping. Inclusive last-byte validation supports the final byte of a 2^64-byte memory without wrapping an exclusive end. |
| table | flags 0; address bits 32/64, current size, minimum, maximum | Length zero: one canonical TYPE-ONLY reference descriptor (flag one, all carrier fields zero), never a default VALUE. Length nonzero: one initialized actual typed reference default value, overridden by table chunks. Exact declared type is retained in both cases. |
| table chunk | flags 0; unsigned element offset | One table ID; nonempty typed reference values exactly matching the table descriptor. Ascending/nonoverlapping for each parent. |
| global | flags 0 immutable / 1 mutable; words zero | One exact typed value. |
| tag | flags 0; function type index | One module ID. Cross-module imported tags retain store identity, not merely structural type equality. |
| data | flags 0 live / 1 dropped; words zero | Exact live segment bytes; dropped segment empty. |
| element | flags 0 live / 1 dropped; words zero | Exact typed segment values; dropped segment empty. Original declared segment type comes from module metadata. |
| structure / array | flags 0; exact local defined type index | One module ID; exact initialized fields/elements, including packed numeric widths and reference identity. Allocator/mark/lock state is rebuilt, not copied. |
| exception | flags 0; words zero | Tag ID, optional original exception-trace ID; complete exact payload values. Fresh native exception publication/token is recreated by the runtime. |
| external wrapper (kind 15) | flags 0; all words zero | Exactly one nonnull typed internal any reference (host, struct, array or i31). No links or bytes. The inner object ID or i31 bits preserve conversion identity. Another wrapper, function, exception, numeric value or null is invalid. |
| host reference (kind 24) | flags 0; nonzero adapter-local logical object ID | One host-resource adapter ID and approved versioned snapshot bytes; no values. Bare `ref.host` has heap any. Unknown host identities fail; native pointers never act as adapters. |
| thread | flags 0 runnable / 1 parked import / 2 atomic waiting / 3 terminated; logical thread ID, scheduler ordinal | Root-to-leaf frames, optionally one actual atomic wait record. Thread IDs are unique and frames/waits have exactly one owner. Host imports must have a fully virtualizable suspended continuation or finish before capture. |
| frame | flags 0 before-opcode / 1 awaiting call return / 2 EH continuation; opcode offset, local count, operand count, control count, handler count, function generation, caller return offset | Function ID then outer-to-inner controls then handlers; exact locals followed by operand values. No native PC/SP/register bits. These must be real compiler materializations, not DWARF guesses. |
| control | flags 0 block / 1 loop / 2 if / 3 function label; entry offset, end offset, captured parameter count, result arity | Actual captured entry values. Label/control types and stack heights come from the exact validated compiler continuation metadata. Result values are not fabricated before execution. |
| handler | flags 0 catch / 1 catch_ref / 2 catch_all / 3 catch_all_ref; label depth, target offset, in-flight exception ID or zero | Catch/catch_ref has exact tag ID. A live exception reference is retained and re-published, not a C++ unwinder stack blob. |
| atomic wait | flags 0 finite deadline / 1 infinite; aligned byte offset, width 4/8, expected bits, virtual deadline | One shared memory ID; actual scheduler queue/order and typed suspended atomic continuation are required. |
| host resource | flags 0 pure / 1 replay input / 2 virtualized side effect / 3 original builtin binding DATA; adapter ID, version, logical resource ID | Approved adapter snapshot bytes. OS descriptors/HANDLEs, absolute host paths or network connections are not restored from wire values. |
| event | flags 0 import / 1 clock / 2 random / 3 scheduler / 4 wait-notify / 5 allocation outcome / 6 floating-point result / 7 extern adapter / 8 effect; sequence, logical instruction, logical thread, adapter operation, argument count, result count, error bits, effect ordinal | Exact capability object IDs, typed arguments/results, approved adapter payload. Sequences strictly increase. Interpreter opcode observations cannot act as these actual event witnesses. |
| exception trace | flags 0 complete / 1 explicitly truncated; words zero | Actual logical function IDs and same-length i64 Wasm offsets, retaining original throw provenance for detailed uncaught diagnostics. |

Mutable continuation records have one owner: every frame and atomic-wait record
belongs to exactly one thread, and every control and handler belongs to exactly
one frame. Duplicate links, cross-frame sharing and orphan continuation records
fail structural validation. Shared memories, store aliases, GC cycles and exception
objects retain their independent legal aliasing rules. A handler depth must name
one of its owning frame's controls. An atomic-waiting thread has exactly one wait
record after all frames; runnable/import-parked threads have none, and a terminated
thread has no frames or wait. These graph checks do not issue executable restore,
scheduler or native stack authority. The state6 wire layout remains unchanged.

Sparse memory/table records do not reduce the runtime's reservation or restore
resource requirements. The manager checks file/disk/address-space/heap quotas and
the actual platform limit before capture or restore. Unsupported enormous live
allocations produce an explicit resource failure. Default codec limits are
256 MiB files and 1,048,576 objects and may be raised only by trusted policy.

## Database transaction and mmap lifetime

The database is an append-only collection of immutable checkpoint `.dmp` objects
and a separately committed branch catalog. Names are generated by fast_io concat
and decimal manipulators from manager-owned checkpoint IDs. They are never guest
paths. The trusted manager owns an opened directory capability, not a string
prefix check. It must exclude that directory from guest preopens and register
each retained directory/file identity in the sealed management asset registry.
All guest path_open, inherited FD publication, aliases, hard links, truncate,
rename and write must reject the actual management assets before mutation.
Ordinary POSIX permissions alone do not isolate a guest running in the same
process/UID with a broad filesystem preopen.

Create a fresh private temporary with native_file RAII and exclusive no-clobber
creation (0600 / private Windows ACL). Hold the real sealed identity through
draining all possible guest accesses. Write with `fast_io::obuf_file`, finish
the footer, explicitly flush its output buffer, request real OS file sync,
check close, atomically publish without overwriting an existing committed ID,
and sync the parent directory with a qualified filesystem/provider. Commit the
catalog only after the object is durable to the requested level. Crash leftovers
and partial footers are unpublished, never accepted as checkpoints. Failed writes
leave the previous catalog and current VM untouched. This helper's current
`os_file_sync` result does not claim power-loss durability: Darwin F_FULLFSYNC,
Windows publication guarantees, actual storage/FS behavior and directory sync
need actual provider qualification before promising that stronger level.

Use `fast_io::native_file_loader` only with an immutable-read owner that prevents
every writer and truncation for the mapping lifetime. MAP_PRIVATE, a checksum,
one before/after fstat, readonly descriptors and a renamed file are not such
proofs: a concurrent truncation can fault before validation. On Linux use a real
sealed memfd backing, populated through fast_io and verified WRITE/SHRINK/GROW
seals; on other platforms use an owner-private anonymous snapshot mapping or an
actually enforced immutable file capability. A source database file is copied
into that backing by bounded nonthrowing fast_io reads under held asset ownership.
Unknown foreign writers require the copy path or rejection. Never expose a live
file mapping that a guest can reopen and truncate. No OS-wide signal handler is
used to hide this race. The codec accepts only a synchronous immutable span or an
already-owned loader; it does not open untrusted filenames or mint file sealing.

## Restore, replay and reverse execution integration

Add a full-only debug checkpoint compiler option and cache identity including
schema/continuation/scheduler/import-policy versions. Debug code must preserve all
Wasm locals (including optimization-dead locals), live operands, control/EH labels
and call continuations in typed logical shadow frames. Each before-opcode boundary
has an exact offset and generation-indexed resume entry. Calls, tail calls, return,
throw/catch, imported calls and atomic waits update logical frames atomically.
Refs remain actual pinned runtime roots until the private capture converts them
to IDs. Native code still validates+translates in one pass. No new check or shadow
write enters ordinary full/lazy/interpreter/tiered execution.

Restore parses and validates into a detached new canonical instance domain,
checks feature/config/module/body/signature/adapter compatibility, preflights
quotas, allocates all GC shells, fills typed graph edges, rebuilds memory/table/
global/segments/import maps, publishes fresh exception objects, and reconstructs
logical frames and the virtual thread scheduler. No old instance byte is mutated
while those operations may fail. Actual world-stop plus exclusive generation
ownership permits one commit replacing the instance domain. Old stop tickets,
native handles, DWARF copied values, code PCs, breakpoints bound to obsolete code,
PC-to-Wasm maps, roots and registry tokens become stale. Recompile/rebind metadata
only from the new actual code publication. Resume uses verified compiler
dispatchers for logical continuation IDs; never branch to wire native addresses.

Record every host/import result, host memory mutation, external-reference event,
clock/random input, allocation failure, permitted floating-point nondeterminism
and scheduler choice at its actual semantic boundary. Shared-memory execution in
record mode uses a deterministic manager-owned single runnable guest participant
per quantum, with instrumented access/boundaries and recorded wait/notify ordering;
recording atomic instructions alone cannot replay races on ordinary shared loads.
Untracked asynchronous external memory mutation is forbidden. Replay verifies
each exact instruction/thread/adapter/argument record before consuming it and
applies only recorded virtual effects. It must not contact the network, rewrite
real files, spawn host processes or repeat an irreversible host effect.
The WASI P1 adapters need a private virtual FS/output overlay and recorded input,
FD state, clock and random results; this is not a WASI P2/P3 implementation.
Foreign imports without complete snapshot/replay adapters fail admission.

Reverse-step/continue selects the nearest checkpoint on the current branch's
ancestry and replays forward to the precise logical instruction or validated
breakpoint. Source/native reverse stepping additionally maps the target using
the restored actual compiler metadata; optimizer-coalesced native PCs cannot
invent missing Wasm state. See [rr's design](https://rr-project.org/) and
[QEMU record/replay](https://www.qemu.org/docs/master/system/replay.html).
QEMU isolates replay side effects with overlays and disables live backend
network interaction during replay. External-resource restore needs explicit
ownership/adapters rather than resurrected numeric descriptors, as illustrated
by [CRIU inherited FDs](https://criu.org/Inheriting_FDs_on_restore).

Mid-run checkpoint enabling is safe only when the actual active code generation
already has complete materializations, or at a drained root-entry boundary after
compiling its checkpoint version. An optimized activation with missing values
cannot be reconstructed from a debugger source display or observation trace.
Previously unrecorded time/random/network/scheduler events cannot be retroactively
invented; reverse capability begins at the first admitted recording checkpoint.

## Acceptance

Standalone components must pass O3, ASan/UBSan and no-C++-exceptions builds on SSH
Linux in the 64 GiB cgroup, then real little- and big-endian QEMU targets with
byte-identical files. Cover every byte truncation, reserved/unknown fields,
oversized counts, digest corruption, reference-kind confusion, GC self-cycles,
alias identity, u64 final-byte endpoints, table64 indices above 2^32, packed and
v128 values, NaN/signed zero, exception tag/payload/trace, dropped segments,
thread/frame ownership, wait queues, branch ancestry and replay divergence.

Runtime acceptance additionally captures a nontrivial active recursive/cross-
module call graph; restores and resumes to identical output; repeats this across
GC and EH/tail-call boundaries and deterministic shared-memory schedules; checks
each import adapter/side effect; rejects unauthorized guest requests/FD aliases;
injects writer/truncation/crash/allocation errors; verifies atomic restore;
and checks no normal-mode code/performance changes. Windows/Linux/macOS providers
must pass their real qualification tests before their capabilities are reported.
The new `debug_checkpoint_core3.wat` exercises new Core 3 constructs; parser/codec
or design tests alone do not count as full-instance runtime acceptance.


State6 diagnostic provenance
---------------------------

State6 also preserves the original immutable diagnostic trace attached to each
actual Wasm exception. A trace object with `words[0] == 1` has one value and
function link per frame; each value is an i64 diagnostic instruction offset.
`UINT64_MAX` explicitly means the producer did not record an instruction
position. It is never instruction offset0, a native PC or a resume selector.
A genuinely unavailable original function generation is encoded as 0 in the
trace payload and is distinct from the current callable function generation.
The current exception producer supplies module/function identity and names,
but supplies neither an original instruction position nor original generation.
A snapshot must not infer those fields from current code after hot replacement.

Trace bytes contain one canonical record per ordered frame link, each made of
five little-endian u64 fields: origin instance ID, public function index,
original generation (currently required to be 0), original module-name byte
length and original function-name byte length. The exact two original name byte
sequences follow each40-byte header in that order. Empty names are preserved;
no locale conversion, UTF8 repair or regenerated current name is applied.
The origin instance's public function slot must point to that frame's function
link, even when the imported callable aliases a provider in another instance.
The trace's truncated flag, exact order, original identity and complete names
remain separate from current code provenance. A trace with payload revision0
has no name payload and is DATA only; the new actual complete census emits
payload revision1. Version6 binding and the actual compilation profile's schema
field6 prevent a version4 payload/cache from silently using this new meaning.
The outer envelope remains version4. Its encoding version is independent of
the actual native continuation ABI2 required by the new original-prototype
resume entry. The compiler profile field3 and actual binary build continuation
revision must both equal that genuine native ABI; file versions never authorize
an executable continuation or convert an old ABI1 plan into ABI2.

The complete census starts a fresh recording origin at its actual accepted
cooperative stop: checkpoint1, no parent, logical instruction0 and event cursor0.
These zero counters denote no later recorded instructions/events, and are not
historical execution counts. Its thread ordinal is the retained logical roster
order, not an OS thread ID, a host runqueue position or proof of a replay choice.
Subsequent deterministic replay needs an actual scheduler and import-event
producer. A private typed graph copy does not itself issue a restore, resume,
resource epoch change, file capability or rollback of external WASIp1 effects.
The required-features mask is the union of compatibility requirements derived
from each actual owned module's resolved enabled policy. Disabled GC/threads do
not become requirements just because the product implements them. This mask
cannot reproduce validation policy: the actual module flags1 tuple records CLI
mode (MVP grammar selection), nineteen disable flags and both legacy controllable
restrictions. It is copied from that module's immutable parser parameters after
real source/member/epoch proof and is independently serialized with fast_io into
the actual full/partition cache context. Its real original publication identity
remains owned while replacements are validated under the same source policy;
replacement generation/body ownership is authenticated independently.
Nineteen disable bits, in increasing order, are multi-value, reference types,
table instructions, multiple tables, bulk memory, sign extension, nontrapping
float conversion, SIMD, extended constants, table initializer, relaxed SIMD,
multiple memories, tail call, memory64, table64, function references, GC,
exceptions and threads. Controllable bit0 restricts multi-result vectors and bit1
restricts multiple tables. Ordinary CLI ownership bookkeeping is not executable
validation policy. Parser quotas remain a separate trusted source-preparation
policy; this tuple does not claim to serialize them. A future fresh-world issuer
must compare this exact per-module tuple to actual parser/validator inputs,
validate all original modules/effective bodies under it, and match accepted cache,
product/build, native ABI and code-profile bindings before executing. No such
whole-world issuer is supplied by this DATA census. Legacy module flags0 alone
never supplies the missing original syntax policy for executable restoration.

State6 original builtin import binding (DATA only)
------------------------------------------------

`host_resource.flags == 3` records the original builtin code ABI, not a pure
import, replay input, virtualized effect, host lifetime or restoration adapter.
Its only recognized kind is `0x3150495341575755` (LE word UWWASIP1), revision1,
and original known-function index+1 (index0..127). Every unused word is zero.
There are no links/values. Exactly108 bytes are: ASCII `UWVMWASIP1B1` (12),
function index LEu64, interface revision LEu64=1, whole ordered builtin-interface
SHA256 (32), parameter count LEu64, result count LEu64,16 parameter type bytes,
16 result type bytes. Counts are0..16; active types are i32=0x7f/i64=0x7e,
unused types are zero. A nonzero hash is a structural DATA check only.

The genuine producer proves the original immutable import declaration, any real
initializer-owned name rewrite with the same original type, the exact terminal
import member, current validated source epoch and actual final builtin loader
wrapper/known tuple before copying this descriptor. Imported functions use
flags1, original generation1, original terminal public index/type, and the real
terminal receiving-instance ID plus this binding-resource ID. Exact alias
chains may reuse that terminal function; different terminal direct imports in
different instances remain different functions even when descriptors match.
Unknown DL/native providers still require their genuine adapters and are denied.

State6 function-generation semantic identity includes the exact binding bytes,
flags, kind/revision/index. It never equates equal ABI arity with equal providers.
Bindings cannot be a `host_reference` issuer or event capability; the replay
log explicitly refuses a graph containing them without real effect adaptation.
Actual VM restore/world preparation remains subject to its independently minted
host resource/lifetime/ownership capability; this document grants none. Schema5
is rejected, never migrated into these absent original provenance fields.
Profile tuple field2 is6; actual native ABI2, profile res17/observe12, identity2,
envelope4 and cache5 remain independent and unchanged.
