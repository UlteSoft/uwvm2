# UWCPDB4 identity-bound file DATA (private source proposal)

The envelope adds strong typed identity DATA to the existing canonical Core 3
state schema. It is a file codec, not a whole VM checkpoint implementation.
Neither decoding success, a supplied target manifest, a file digest, a filename,
a native source pin nor a cache key grants stop/census/restore/replay authority.
Complete live GC/EH/thread/host state capture and executable typed continuation
restore remain separate unfinished runtime requirements.

All integers use fast_io fixed-width LE put/get. Both real little and big endian
hosts must produce identical bytes. No C++ padding, native pointers, JIT PCs,
OS handles, allocator headers or native stack/register blobs occur on wire.
Original Wasm bytes include custom and data sections, not just translated IR.
The distinct host/extern-wrapper/GC/i31 identities and complete logical state
representation retain state6's structural validation; the file codec does not
supply the missing actual runtime exporters or type/continuation verification.

## File layout

The 160-byte outer header is:

| Offset | Field |
| ---: | --- |
| 0 | u64 magic `UWCPDB4\0` |
| 8 | u16 envelope 4, u16 header size 160, u32 endian marker 0x04030201 |
| 16 | u64 flags zero |
| 24 | u64 identity bytes |
| 32 | u64 complete canonical state6 document bytes |
| 40 | u64 ordered original module count |
| 48 | u64 ordered cache object count |
| 56 | u64 reserved zero |
| 64 | 32-byte SHA256 of canonical identity bytes |
| 96 | 32-byte SHA256 of the complete canonical state6 document |
| 128 | 32 reserved zero bytes |

Then come canonical identity bytes, complete state6 document, and a 48-byte
footer: u64 `UWCPEND4`, u64 exact prefix length (header + identity + state), and
SHA256 of that entire prefix. Hashes detect corruption and exact identity;
they are not a signature, request authentication or a VM resource credential.

Identity is exactly the existing `binding::canonical_digest` preimage, including
its `UWCPID02` domain: 484 fixed bytes + 124 for each ordered module + 240 for each
ordered cache object. Six exact build digests, product, compatibility/continuation
versions, all ten immutable profile fields, target ABI, source closure, saved
canonical state document digest and recorded event-prefix digest are retained.
All original module objects in state6, in increasing file-local object ID order,
match this source-role closure one to one by count, complete byte length and
whole SHA256. No unused/uninvoked dependency is inferred from live frames.

Identity revision2 recomputes every effective function body/generation closure
and the saved full replay-log/cursor digest before publication or decoded output.
The event-prefix digest remains DATA; it is not independently a proof of host
replay completeness. A real saved-state producer must derive it
from its actual admitted journal/cursor and prove its serializers/effect policy.
Opaque host imports without that model are not replayable.

Required cache mode has every actual accepted entry/blob/object/context/provider
identity, in complete ordered full-module or full-partition bundles. A predicted
key, request path, detached cache-format test object or fallback compilation is
not an accepted cache receipt. Actual debug-full currently disables lookup/store;
its future producer must record observed `off_debug_process_binding`, never
fabricate a cache hit or omit the exact OFF reason. An ON/OFF, OFF-reason,
partition, product, same-version/different-build or original-byte mismatch denies
compatibility. No silent regeneration/rebinding or legacy migration occurs.

## Bounds, integrity and failure atomicity

Before any file-derived allocation, reader checks live immutable input extent,
PTRDIFF limit, minimum header/footer, magic/version/endian/reserved fields,
quotas, counts, exact identity-size formula with subtraction/division before
multiplication, remaining state length and exact footer prefix. It checks all
three complete hashes before identity vectors or state graph allocation.
Canonical identity shape/hash and its state digest are checked, then state6's
own checksum/limits/graph checks. Reencoding the detached state proves exactly
one canonical state document, and complete embedded-source bytes are compared
against the identity before comparing the target environment.

Default file budget is 256 MiB, identity budget 32 MiB, with independent module,
cache-object and state6 object/link/value/thread/frame quotas. These bound codec
owned DATA, not process RSS, parser scratch, native stack or live VM resources.
Cold encoding/reencoding may retain several bounded buffers simultaneously.
Allocation/IO exceptions use the surrounding build's existing policy; until
successful final move no prior output DATA is modified. There is no claim that
`catch` changes noexcept fast_io allocator behavior.

`load_file` uses actual RAII/no-throw fast_io operations through the qualified
private `owned_file_image` reader, copies before parsing and retains that private
nonmoving immutable allocation throughout decode. It does not mmap a live file,
reopen it after parsing, or treat fstat/checksum as protection from truncate
SIGBUS. The reader checks status/type/size, EINTR/partial reads/EOF/growth and
checked close. Unsupported providers remain explicit read failures. Private
bytes remain readable after a disposable fixture path is overwritten/truncated.
They do not prove external file atomic-version provenance or guest isolation.

Output is an unpublished trusted stream only. Publication still needs real
non-guest asset owner/FD/preopen/alias census, complete stop + host operation
closure, permissions, RAII ownership, checked flush/close, appropriate file/directory
sync and no-clobber transaction publication. No directory, asset, save or restore
capability is acquired by this codec. File DATA can be deliberately forged;
authoritative source/build/cache producers remain mandatory before any VM action.

## Saved body versus target environment

`target_environment_data` contains exact source/build/cache/profile/ABI identity
DATA and has **no current-state or saved-state body hash field**. The reader
compares the verified saved environment with the future actual target publisher's
identity. The already verified saved body/event hashes are composed into that
comparison internally; it never requires the target's current mutable state to
equal the old saved body. Two different verified saved bodies under one unchanged
environment are accepted as DATA. They still do not grant executable restore.

An unbound current state6 file (`UWCPST6\0`) or old state3 file (`UWCPDB3\0`)
receives `legacy_unbound_state_data` at this entry and never becomes identity-bound.
The current structural `decode_database` supports explicit state6 DATA inspection
only, after its own bounds/hash/graph checks; state3 is unsupported and is not
migrated by guessing a missing empty-table value. No mode implicitly treats an
unbound state file, an unknown schema or an incompatible cache as
an executable checkpoint. Target build/source/cache receipt authentication and
whole runtime resource/continuation transaction are pending.

## Fresh finite tests

New C++ fixture uses actual private owned files, real signed cache-format blobs
from the existing complete-bundle fixture and actual Ed25519 verification. Three
modern Wasm files include recursive GC types, nondefaultable i31 local, mem64,
table64 and custom debug bytes. Custom-only C has exactly the same non-custom
sections as A; the independent Python oracle checks that on official wasm-tools
encoded files. No actual loaded-engine cache acceptance or physical build proof
is inferred from these component files. The structural graph includes GC alias/
cycle, packed fields, v128/NaN/signed-zero, host/extern wrappers, exceptions/EH,
nondefaultable unavailable locals, 64-bit sparse memory/table and thread/wait
records. It is deliberately DATA and is not asserted to match a real running VM.

Finite source plan has 40 fresh negative cases plus every file truncation:
wrong original/custom-only source, cross product, all six build closures with
same visible version, ABI/profile workspace, module/replacement closure, all
three actual signed blobs/partition order/absence/duplicate/format, ON↔OFF and
OFF reason, unknown envelope/identity/state schema, overflowing count/length,
checksum/body corruption, malformed graph after attacker recomputes **all**
checksums, embedded module mismatch after all hashes are repaired, unbound state6 or legacy state3,
quota and writer rejection before any output. Both distinct saved bodies use one
target environment. File/private-copy overwrite is tested only on a disposable
scratch file. The independent Python model produces complete expected bytes for
two saved bodies and compares full files, not just a native-produced checksum.

These tests have not run in this source proposal. Actual C++/oracle/native/BE/
Windows/BSD/BMI/whole runtime qualification is false until the Linux keeper's
bounded job or the platform keeper's real target execution produces receipts.

References: [Core 3 runtime values](https://webassembly.github.io/spec/core/exec/runtime.html#syntax-ref),
[Core 3 reference conversions](https://webassembly.github.io/spec/core/exec/instructions.html#syntax-instr-extern),
and the repository's `checkpoint_format.md`, `checkpoint_binding.md` and actual
cache-format version/provider source. WebAssembly specifies guest state semantics;
UWCPDB4 is this project's explicit file contract, not a WebAssembly wire standard.

The precise revision2 function/event preimages and pending corruption regressions
are specified in `test/0036.checkpoint_binding/README.state-identity.md`.

The inner state6 magic/commit are `UWCPST6\0` / `UWCPSTE6`, distinct from
the outer envelope4 `UWCPDB4\0` / `UWCPEND4`. The inner header remains128,
outer160. A zero-length table has only its exact declared reference type in a
canonical flag-one, zero-carrier descriptor; it is not an initialized null,
element, frame operand, GC field or retained root. Empty/nonempty descriptor
rules are checked by the state graph validator before output publication.


State schema6 additionally binds preserved exception diagnostic origin/name
payloads and the explicit unknown instruction/generation markers. Inner state
magic and commit are `UWCPST6\0` / `UWCPSTE6`; state5/4/3 are never silently
migrated. Envelope4, cache format5 and identity2 remain unchanged. The complete ten-field
actual compilation profile records schema6 and separately the genuine native ABI;
profile policy versions are independently preserved, not reset by this bump.

Controlled schema6 adds original builtin WASIp1 binding-only flag3 resources.
Their exact108-byte code-interface descriptor participates in the effective
function-generation digest. Schema5 does not provide that provenance and is
never silently upgraded. This changes the inner state ST6/STE6 and actual profile
field2 only; identity2, envelope4, cache5, native ABI2 and policy res17/observe12
remain independent. Equal detached descriptors grant no host lifetime, effects,
replay or external restoration rights; actual adapters remain mandatory.
