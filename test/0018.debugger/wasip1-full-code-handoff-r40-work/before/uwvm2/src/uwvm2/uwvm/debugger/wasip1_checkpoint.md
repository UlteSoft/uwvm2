# WASIp1 resources and Wasm checkpoints

## User-visible contract

A Wasm graph checkpoint does not include WASIp1 descriptors, argv, environment
variables, file contents or external I/O effects. When the current instance
uses builtin WASIp1, `llvm_jit_checkpoint_capture_instance_host_api` now prints:

> checkpoint reminder: capture WASIp1 together with Wasm at the same cooperative stop; Wasm-only snapshot omits FD/environment state.

Its result also carries `wasip1_checkpoint_required=true` and
`wasip1_captured_together=false`. Callers should display this reminder even when
stderr is not their user interface.

Use `llvm_jit_checkpoint_capture_instance_with_wasip1_host_api` to capture the
complete logical Wasm graph and every distinct visible builtin WASIp1
environment under ONE actual cohort/pause, host admission closure, GC exclusion
and publication guard. Shared environments are captured once. Every capsule
uses the graph's recording label and observed runtime generation. The result
publishes the graph and capsule roster only after all captures succeed.

This joint capture is a live process artifact. Serializing `result.graph` alone
still omits the environment capsules; it must not be advertised as a persisted
WASI checkpoint. Capsules are not executable new-world publication authority.
This change does not implement whole-instance Wasm restore or an atomic joint
Wasm-and-WASI restore dispatcher.

## Debugger commands

All resource edits, snapshots and restores require a current genuine cooperative
LLVM JIT full stop. Native instruction stops cannot access these operations.
Existing `if-stop STOP_ID` comparisons are checked by the controller before a
mutation. SLOT is 0..7, local to the controller. Saving an occupied slot replaces
it only after successful capture and validation of the detached reply metadata.
Failed capture or metadata allocation leaves the old slot intact. Restore and
drop validate that metadata before changing anything. A metadata allocation
failure is reported as `allocation failed`, rather than a missing checkpoint.
All three operations report the saved recording's `runtimeEpoch`, including
drop after releasing its slot. Restore uses the validated scalar metadata
without a second allocating copy after the live commit.

```text
# An anonymous file, containing A, NUL, B. '-' creates an empty file.
set wasip1 file 0 410042
# The reply identifies its guest FD. Use its actual queried rights below.
info wasip1 fds 0
set wasip1 fd-dup 0 FD 0x60006e 0
unset wasip1 fd 0 FD 0x60006e 0

set wasip1 checkpoint 0 0
set wasip1 restore 0 0 bindings
set wasip1 restore 0 0 strict
unset wasip1 checkpoint 0 0
```

FD duplication shares the resource, cursor and file flags, preserving both
rights masks. Close removes a guest binding, not a host pathname. Snapshot
owners retain their required resources independently of subsequent guest close.
At the 65,536-cell debugger scan limit, construction and duplication may still
reuse a closed dense slot. They refuse an allocation that would grow the table;
reserved empty slots remain occupied and cannot be reused.
Construction does not accept native handles or host paths. The anonymous
provider uses the OS-specific FastIO storage described below; rights are bounded to
read/write/seek/tell, flag changes and file size/stat operations (`0x60006e`),
with no inheriting or directory/network authority. The console debugger's empty FD0 placeholder remains reserved on restoration; it never becomes a
guest capability for management input.
The factory clears Linux FastIO's default `O_APPEND` before publishing the file,
so ordinary positioned guest writes work initially. Initial payload writes finish
with cursor zero on every OS. Guest-visible flag edits are included in the saved
managed state. Reserved empty cells also count toward the current
host FD limit during restoration.

A resource reply includes `wasip1-fd affected=NUMBER`. Snapshot replies include
managed and retained external resource counts and `external-io-rollback=false`.
They remind the user to checkpoint Wasm and WASIp1 at the same stop; a WASIp1-only
restore leaves Wasm memories, globals, stacks and other logical state unchanged.
Restoring a WASI capsule captured at a different stop from a Wasm graph is not
a joint checkpoint. Prefer the joint native API when coordinating both domains.

The existing structured DAP request `uwvm/wasip1Edit` supports:

| operation | fields |
|---|---|
| createFile | value or valueHex; binary NUL accepted |
| duplicateDescriptor | descriptor, expectedBase, expectedInheriting |
| closeDescriptor | descriptor, expectedBase, expectedInheriting |
| saveCheckpoint | slot |
| restoreCheckpoint | slot, resourceMode: bindings or strict |
| dropCheckpoint | slot |

All accept `moduleId` and optional `stopId`. Both resourceMode values must be
chosen explicitly. Replies preserve `applied=true` if a successful commit is
followed by an independently resumed stop. The adapter invalidates old stop
references before sending an edit and verifies acknowledgement suffixes rather
than ignoring extra output. Checkpoint replies expose `wasmCheckpointRequired`,
`managedResources`, `retainedExternalResources`, `externalIORollback=false`.
For save, restore and drop, an `ok` acknowledgement must confirm `applied=1`
and include the matching slot, bounded resource counts and the reminder to
checkpoint Wasm and WASIp1 at the same cooperative stop. Missing or malformed
metadata, a zero recorded epoch on success, or `ok` with `applied=0`, is a
protocol error; cached stop references
stay invalidated and the adapter does not retry the operation. Inspect state
before deciding whether to retry, since a malformed reply does not prove that
the native operation failed. A non-`ok` refusal with `applied=0` may omit the
checkpoint suffix if admission rejected the request before that stage.

## What restoration means

| state | bindings mode | strict mode |
|---|---|---|
| Owned argv/env text and views | restored together | restored together |
| Guest FD map, reserved empty slots and closed-slot allocation order | restored | restored |
| Descriptor rights and aliases | restored from authentic snapshot | restored |
| Managed anonymous file bytes, length, offset, flags | restored | restored |
| Owned external file/directory/socket binding | retained original resource | rejects snapshot before commit |
| Borrowed native observers | restore rejected | restore rejected |
| Real pathname contents, directory operations, kernel pipe/socket buffers | unchanged | unavailable |
| Real stdin consumption, emitted output, network messages | unchanged | unavailable |
| Kernel-generated inode/time/stat metadata | not rewound | not rewound |
| Clock/random/poll results or deterministic event replay | not included | not included |

`strict` means every captured descriptor is managed by the anonymous resource
provider. It is not a claim to rewind all kernel state or produce deterministic
WASI replay. Stdio and preopens normally count as retained external resources,
so a normal environment will fail strict restoration until those bindings are
removed or supplied by a future virtual provider. Failed strict restoration does
not alter argv, env, FD mappings or managed content.

Managed files use real anonymous regular files for the existing WASIp1 ABI.
Guest read/write/seek/truncate operations therefore use the original
implementation without a parallel simulated syscall path. A restore materializes
ONE fresh backing file per saved managed resource and points every saved alias
at that resource. Snapshot bytes are immutable; retaining or dup'ing an old
native FD alone would share changing offsets/content and is insufficient.
Snapshot inode/time metadata is not virtualized in this provider.

Ordinary external owned resources preserve their original identity through
private retained owners. They are not reopened by pathname; pathname replacement
must not silently substitute another resource. A copied descriptor number,
metadata struct, resource ID, native pointer, or shared_ptr with a foreign control
block cannot authorize restoration.

## Preparation and commit

1. Authenticate every before-park capture through the canonical owner registry;
   authenticate the actual complete current cooperative cohort, generation,
   source/provider provenance, host exclusion, GC roots and publication.
2. Canonicalize the saved capsule by both address and control-block ownership.
   Verify its original module, current generation, original Wasm hash, builtin
   interface hash and private environment identity. Historical addresses are
   compared only; they are never dereferenced to recover an environment.
3. Reject strict external resources, observers, quotas or mismatched bindings.
   Allocate fresh managed files and restore their bytes, length, offset and flags.
   Construct candidate argv/env backing and views, FD map and free list.
4. Verify the current host FD limit. Never restore a larger host policy limit.
5. With all real actors excluded, swap the prepared table and text into the
   current environment. No fallible native I/O, callback or allocation occurs
   after the first swap. Retired owners may close their handles through noexcept
   destruction. Preparation failure leaves the published environment unchanged.
6. Report a committed restore even if cancellation is requested afterward.

The runtime-only managed identity lives in the resource RC object, so duplication
and renumbering preserve it. Identities issued by construction never wrap/reuse;
restoration preserves a historical logical identity on its fresh backing. It is
metadata, not a lookup or authorization token. Guest FD reuse is resolved using
the capsule's private bindings and retained authentic owners.

Limits: 8 controller slots; 16 simultaneously live registered capsules;
65536 descriptor scan cells; 1 MiB owned environment text; 16 MiB per managed
file and 64 MiB aggregate payload per capsule; 4096 initial file bytes per console
or structured create request. Aggregate payload counts unique resources, not
aliases. Registry ownership is weak, so dropping final owners releases retained
resources. Copied data remains diagnostic only.

## Backend scope and extension points

The resource factory has native Linux, macOS, FreeBSD and Windows implementations.
All file I/O, entropy, filename formatting and filesystem operations use FastIO.
The common resource transaction retains the existing platform-typed WASIp1 storage.

| OS | Managed backing | Saved flags |
|---|---|---|
| Linux | FastIO `io_temp` / `O_TMPFILE`, with default append cleared | WASI-visible POSIX flags |
| macOS / FreeBSD | FastIO directory-relative exclusive 0600 creation with OS entropy; unlink before payload or FD publication | WASI-visible POSIX flags |
| Windows | FastIO Win32 temporary file with exclusive access and delete-on-close; transfer its owned handle into the NT `native_file` and original WASI flags wrapper | Original ordinary read/write mode and zero wrapper flags |

macOS's `/tmp` directory link is followed when pinning the temporary directory;
individual file creation retains FastIO's no-follow and exclusive semantics.
POSIX restore preserves the new handle's non-WASI flags. Immutable kernel bits,
such as Darwin's post-write bookkeeping bit, are not replayable file flags.
Only append, nonblock and the available sync/dsync/rsync bits enter the snapshot.
Windows' existing WASIp1 implementation rejects changes to append/sync/nonblock
with `ENOTSUP`; managed creation does not invent support for those flag changes.

NT positioned reads on these synchronous handles change the shared native cursor.
Managed capture restores the original cursor after both successful and failed
reads. Restore always writes bytes before setting the saved cursor and flags.
No raw handle is accepted from the debugger and no pathname is reopened to restore
an existing binding. Kernel-generated inode/time/stat metadata remains outside
this checkpoint contract.
The current genuine checkpoint/stop infrastructure selects LLVM JIT full;
uwvm-int full retains its ordinary WASIp1 execution behavior. Both uwvm2 and
uwvm2-ros share this implementation, without adding removed ROS basic modes.

Future providers can implement a versioned virtual filesystem with pathname,
directory, timestamps and logical inode state; virtual stdin/stdout; recorded
clock/random/poll inputs; and explicit external-resource replay adapters. A
persisted checkpoint needs provider reconstruction data and a separately trusted
capability resolver. Arbitrary serialized paths/FD integers must never mint host
access. A future whole-world restore must stage this resource transaction inside
the actual world publication transaction, rather than restoring Wasm and WASI
with two independently resumable management calls.

ASM debugging remains confined to generated Wasm context. These WASI management
operations cannot inspect host/VM memory or grant an ASM debugger host execution
access.

## Native shutdown and console isolation

A completed guest callback is not an OS thread-retirement receipt: C++ TLS
cleanup can still borrow runtime and code owners. Managed CLI shutdown keeps
those owners until the original native thread has actually joined. The finite
join provider is Linux/glibc `pthread_tryjoin_np`, Windows NT thread wait,
FreeBSD 15.1+ `pthread_tryjoin_np` (selected using the actual SDK `sys/param.h`),
and macOS kernel-authenticated Mach dead-name delivery followed by the original
`pthread_join`. The macOS watch is armed while the new thread is suspended; it
pins its own rights and validates the kernel audit trailer. A pending/error
result retains ownership. No body-done bit, joiner thread, or guessed native
handle is a retirement acknowledgement.

### Retaining a prepared candidate through real native retirement

The cold native management API
`llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket, captures, request, deadline)`
prepares a fresh private candidate under the same authenticated complete pause,
closed host gate, GC exclusion and publication guard. It requires the original
resumable compilation profile and the actual registered native guest cohort.
Every candidate resource, engine, frame and private worker context must succeed
before any original guest retirement is activated. Aliased capture pointers,
incomplete cohorts and unregistered native workers cannot authorize retirement.

The candidate also prepares final defined-function and import dispatch caches.
The final sorted pointer ranges also bind table/funcref lookup to new-source
storage, with checked extents and no overlapping ranges.
Each defined entry borrows the new source member and the fresh engine's sealed
resume plan, typed entry and function generation. Import resolution validates
all new-source alias hops and keeps the caller module as the value namespace.
Only the authenticated builtin WASIp1 provider can supply native dispatch
metadata; it selects the candidate environment and its new memory binding.
Dynamic or unknown providers still require an explicit restoration adapter.

`maximum_private_dispatch_bindings` bounds the combined number of defined and
imported slots before cache allocation (default 1048576). Payload accounting
also includes the outer vectors and every cache entry. A refusal returns
`dispatch_binding_preparation_declined` before old-worker retirement. Successful
preparation reports `runtime_dispatch_bindings_prepared` and the defined/import
counts. These fields describe private data; publishing the world, starting
restored guest workers and replaying guest frames remain separate unfinished
steps. No global source, dispatch cache or Wasm epoch is changed by preparation.

If WASIp1 is visible to any module, set `request.include_wasip1 = true`.
Omitting it returns `preparation_declined` with
`preparation.wasip1_checkpoint_required = true`; original guests stay paused.
This includes an enabled default WASIp1 environment even when the selected Core
module makes no WASI calls. Strict resource mode remains the default and refuses
external resources such as inherited stdio. Setting
`require_managed_wasip1_resources = false` explicitly permits their retained
bindings; it does not rewind external I/O. Capture Wasm and WASIp1 together at
the same cooperative stop.

| Result | Meaning and next step |
|---|---|
| `preparation_declined` | Preparation failed before old retirement; no operation owner escapes. Read `preparation` and `cohort_diagnostic`. |
| `pending_execution` | Keep the canonical operation; the original execution leases have not all left. |
| `pending_native_join` | Execution drained, but actual OS exit/join, including C++ TLS cleanup, is pending. |
| `prepared_world_ready_closed` | Both drain and physical join succeeded. The manager retains the candidate and keeps ordinary execution admission closed. |
| `retired_and_joined` after explicit discard | Candidate disposal completed and the same original instance was reopened, with its original Wasm epoch. |
| `failed_closed` | Ownership and closed admission are retained; a numeric result cannot grant reopening. |

Call `llvm_jit_checkpoint_continue_native_retirement_host_api(operation, deadline)`
with the same canonical owner to retry either pending phase. A dropped public
owner, completed callback or copied status never frees the retained worlds or
opens admission. Finite deadlines govern drain/join waits; bounded synchronous
candidate preparation is not preempted when that deadline elapses.

To abandon a ready candidate, call
`llvm_jit_checkpoint_discard_prepared_instance_host_api(operation, deadline)`.
An early call remains pending until real TLS/OS death. Discard reauthenticates
maintenance, drain and join, and destroys candidate/provider owners outside the
manager registry/admission locks. Ordinary launch and legacy reset/stop/source
replacement remain excluded while the operation owns the closed generation.

`preparation.status = prepared_and_retained` and
`candidate_world_retained = true` are diagnostics. This API does not publish a
new world, change the runtime epoch or execute a restored frame. New worker
startup enrollment, atomic Wasm/WASIp1 publication and restored guest execution
remain separate unfinished work. These owners are process-local; portable
metadata and file labels cannot recreate their native authority. ASM debugging
continues to be limited to generated Wasm context.

FreeBSD console protection supports genuine terminal slaves and regular files.
PTY masters and input aliases are denied by actual device identity before WASI
publication or truncation; unknown terminal identities fail closed. FreeBSD and
macOS ordinary console input still reject anonymous blocking pipes without an
independently owned cancellable reader. This does not disable the existing
separately authenticated macOS control transport. Terminal settings are restored
on managed exit; tests compare configured flags, speeds and control characters,
accounting for the kernel's transient `PENDIN` retype state.

## Verification

Non-macOS compiler, native and QEMU runs use the SSH Linux original cgroup.
macOS native runs execute locally under the requested 2 GiB accounting limit;
large product translation units can be cross-compiled inside the Linux cgroup.
Only one native architecture per OS is needed for this OS resource boundary.
The report distinguishes production file-provider tests from complete cooperative
runtime/CLI qualification.

- `test/0017.runtime/wasip1_native_file_platform.cc`: actual OS creation, binary
  bytes, native aliases, cursor preservation including failed reads, independent
  materialization, POSIX append/nonblock restoration, Windows immutable flags,
  private anonymous storage and repeated construction/retirement.

- `test/0017.runtime/physical_native_thread_join.cc`: a genuinely blocked TLS
  destructor retains pending ownership; actual release permits the real join;
  moved/swapped/ordinary owners and macOS rights retirement are checked.
- `test/0017.runtime/sealed_console_bsd_platform.cc`: actual PTY master/slave,
  duplicate aliases, regular hard links, pre-truncation rejection, exact null
  sink identity and FreeBSD blocking-pipe refusal.
- `test/0017.runtime/wasip1_debug_cli_platform.cc`: actual public `-Rdbg`, both
  stack policies, managed binary file/FD/checkpoint operations, real resumed
  guest reads, call traces, actual child retirement and terminal restoration.
- `test/0013.debugger/wasip1_checkpoint_commands.cc`: binary construction grammar,
  bounded IDs, explicit restoration modes and existing stop guards.
- `test/0018.debugger/test_dap_wasip1_state.py`: structured binary creation,
  acknowledgement validation, limits and Wasm checkpoint reminder propagation.
- `test/0017.runtime/debug_wasip1_checkpoint_runtime.cc`: real Core3 LLVM-full
  before-park ownership; shared resource identity; immutable bytes; payload
  quotas; strict refusal; FD-policy refusal; argv/env/content/offset/flags restore;
  FD reuse/free-list order; rights rollback; canonical-owner rejection; joined
  capture; real guest WASI reads after resume; retired-pause rejection.
- `test/0018.debugger/test_wasip1_live.py`: real CLI, authenticated Linux broker
  and DAP creation/close/dup/snapshot/restore/drop with both stack strategies,
  plus prior environment and call-trace regression coverage.

Run every native/runtime/live fixture for `instruction` and `unwind` independently
in both repositories. Validate fixture Wasm with wasm-tools first. Ordinary full
interpreter/full LLVM execution and removed ROS mode refusal remain regression
checks. Verification receipts record source identities, provider inputs, binaries,
logs, actual cgroup membership and retirement of this task's own descendants.

Native live-capsule OS qualification: [Linux, Windows, FreeBSD and macOS report](../../../../test/0018.debugger/wasip1_checkpoint_os_test_report.md). It records both stack policies, actual public CLI processes, OS-specific resource/terminal/thread checks, fixed source identities and the local 2-GiB macOS RSS bound. Portable process/OS migration qualification is tracked in the [portable checkpoint report](../../../../test/0018.debugger/wasip1_portable_checkpoint_test_report.md); the frozen-source corrections, full native suites on both repositories and all 48 directed cross-OS restore cases have passed. The report identifies concurrent working-tree dependency changes that were preserved and are not covered by that executed snapshot.

## Portable metadata across operating systems

`set wasip1 export MODULE HEX_PATH` writes a new version-1 metadata file.
`set wasip1 import MODULE HEX_PATH [RESOURCE=TARGET_FD,...]` restores that metadata
in a fresh, authenticated target process. Paths are UTF-8 hex bytes, so spaces do
not become command separators. Export uses exclusive creation: choose a new
filename rather than replacing an existing checkpoint. Native checkpoint filenames
are bounded to 4096 bytes and validated as NUL-free RFC 3629 UTF-8 before string
allocation and native I/O; Windows drive/target path syntax is retained. Both operations require
the genuine cooperative LLVM-full stop and print the paired Wasm/WASIp1 reminder.

```text
# /tmp/wasi.uwp encoded as hex:
set wasip1 export 0 2f746d702f776173692e757770
# Start the same original Wasm on the target OS, configure mnt with the same
# guest mount names and target-native host directories, then pause it:
set wasip1 import 0 2f746d702f776173692e757770
# A refused external resource names its index in diagnostic="resource=...".
# Bind that saved resource to an existing target guest FD:
set wasip1 import 0 2f746d702f776173692e757770 4=91
```

The target path passed to import is native to the target OS. A source guest mount
name such as `portable-root` remains unchanged; its source host pathname is never
saved. The target's mnt configuration selects the actual host directory.
Ordinary files are reopened below that current mount without CREAT/TRUNC, and
their saved cursor and supported WASI flags are applied to the fresh open. This
preserves target file contents, including a cursor beyond the current end. The
new FastIO explicit-disposition mode keeps write-only access and APPEND from
implicitly creating or truncating files. Missing files cause import to fail.

| Saved state | Portable restore |
|---|---|
| Owned argv/env bytes | Rebuilt owners and views |
| Guest FD numbers, aliases, base/inheriting rights | Complete replacement table; one backing resource per saved alias group |
| Reserved empty cells and closed-slot allocation order | Preserved; the target debugger's sealed FD0 cannot be replaced |
| Preopens and opened subdirectories | Resolved through target mnt by guest mount name |
| Canonical mounted file path, cursor, WASI flags | Reopened and verified under target capabilities |
| Original stdin/stdout/stderr identity | Rebound to the target's original channels, including renumbered channels |
| Anonymous/untracked regular file | Explicit target FD required; known mounted targets reopen independently, anonymous targets must already have matching cursor/flags |
| Pipe/socket/other stream | Explicit target FD of matching WASI type and flags; connection/buffer contents are external |
| File bytes, kernel inode/time metadata, directory entry ordering | Excluded |
| Stdio consumption/output, socket packets, clocks/random/poll events | Excluded |

Explicit file binding can redirect a saved mounted resource to a different
currently mounted target file. It does not accept a native handle or elevate
rights. The target must actually hold the requested capabilities in one FD;
a reduced alias cannot hide another sufficient alias, and separate aliases'
rights are never combined. For file/subdirectory reopening, the parent FD's
base rights authorize PATH_OPEN and synchronization options; both child rights
sets must fit that same parent's inheriting rights. Child FD_WRITE does not
require FD_WRITE in the parent's base rights. Ordinary path_open separately
requires parent PATH_CREATE_FILE for CREAT and PATH_FILESTAT_SET_SIZE for TRUNC
before native I/O, following the [WASIp1 rights contract](https://github.com/WebAssembly/WASI/blob/a2b96e81c0586125cc4dc79a5be0b78d9a059925/legacy/preview1/docs.md#rights). Portable reopening never requests either operation.
Portable file/subdirectory reopening always checks
UTF-8, even if the target's ordinary WASIp1 paths permit raw bytes, so Windows
conversion cannot silently select a replacement-character filename. Unsupported
flag semantics are refused rather than silently cleared. Windows currently
accepts zero file flags and native APPEND; POSIX restoration requires its actual
fdstat flags to match after reopening. Stdio channels retain the target channel's
position and native flags; portable metadata does not rewind their I/O state.
Noncanonical file paths containing parent/dot components, Windows alternate
path syntax or untracked provenance require explicit file rebinding. Noncanonical
directory paths are refused during export. Source host paths are not carried
across OS boundaries.

Restoration validates the entire bounded graph and original Wasm/interface
hashes, opens and checks all replacement resources, allocates text and FD
owners, and only then publishes the complete logical environment. A failure
before publication leaves the target text, FD mappings and native cursors
unchanged. This does not constitute an atomic whole-instance Wasm restore;
coordinate the corresponding Wasm checkpoint separately at the same stop.

The structured `uwvm/wasip1Edit` interface adds:

| operation | fields |
|---|---|
| exportPortableCheckpoint | path or pathHex |
| importPortableCheckpoint | path or pathHex; optional bindings: [{resource, descriptor}] |

Bindings are bounded to 64 unique resource indices; no host handle is accepted.
Single and group command admission share the detached decoder's numeric grammar:
resource indices are below 65536, target FDs are at most INT32_MAX, and pairs
must be comma-separated. Duplicate resource indices (including differently
zero-padded numbers), overflow and a trailing comma are rejected before opening
the checkpoint file. An empty list means automatic target selection; different
saved resources may name the same target FD syntactically, but restoration still
checks the distinct-owner rule below. Export does not accept a rebinding list.
Distinct saved resource rows must resolve to distinct target resource owners.
Binding both rows to the same FD, or to different FD aliases of that one owner,
returns unsupported_resource before publication. Supply independent target FDs
with the required rights, cursor and flags. Multiple saved guest bindings that
already reference one resource row keep their original shared owner and cursor.
This prevents a portable import from merging formerly independent files.
Replies report resources, formatVersion=1, contentMode=external,
externalIORollback=false, wasmCheckpointRequired=true, and an escaped diagnostic.
Portable checkpoint filenames must be nonempty RFC 3629 UTF-8 without NUL,
at most 4096 bytes. Single and group import/export requests, the console parser
and FastIO file helpers share this validation. Console rejection happens before
controller dispatch, entropy generation, environment capture or native file IO,
and leaves the parsed request unchanged. WASI arguments and environment values
remain opaque bytes; this filename rule does not restrict their encoding.
Path plus bindings must fit the existing 8448-byte command envelope. DAP rejects
malformed UTF-8 pathHex before transport, mismatched header/resource counts, and
an ok import/export acknowledgement that does not confirm applied=1; rejected
acknowledgements invalidate cached stop references.

Every `uwvm/wasip1Edit` acknowledgement must pair `status=ok` with
`applied=1`; a non-ok status must have `applied=0`. This also covers argument
and environment edits, rights reduction, and managed file create/dup/close.
An ok header without a confirmed edit is rejected, cached stop references
remain invalid, and the adapter does not retry the edit. A complete committed
reply remains available if the host resumes before the follow-up status query;
its `stopCurrent` is then false. Explicit refusals remain inspectable nonapplied
results. Checkpoint metadata and the paired Wasm/WASIp1 reminder are still
required independently.

Native callers use `llvm_jit_checkpoint_capture_wasip1_environment_host_api`:
set `portable_metadata_only=true` for capture and read the immutable `portable`
result. To import, supply `portable_restore` and optional `portable_rebindings`.
The import request recording_label must exactly match the snapshot label; a
nonzero mismatch returns invalid_portable_snapshot before any replacement is
published. This consistency check applies to single and group imports. Labels
remain detached data: the original source/provider and current cooperative-stop
proof are still required.
`wasip1_portable::save_file/load_file` use FastIO files, strings and bounded scan
parsing. The file has fixed-width little-endian fields, explicit version and
reserved bits, and a SHA-256 trailer. Single and group file helpers explicitly
check native close before reporting export success or publishing decoded
import output. A close error propagates as a FastIO error; controller requests
report `native_operation_failed` with `applied=false`. Failed imports preserve
the previous decoded output and do not reach environment restoration. An
export pins the destination directory with a FastIO native handle, exclusively
creates a random staging directory on that filesystem (0700 on POSIX), and
writes a 0600 `payload` there. Only after successful checked payload close does
FastIO `native_linkat` publish the final name. This is atomic creation without
replacement: readers see no target before commit and the complete wire bytes
after commit. An existing regular file, directory or concurrently created target
is never overwritten. If the parent pathname is renamed or rebound during the
operation, publication and cleanup follow the originally pinned directory.
On Windows the open descendant staging handle prevents ordinary NT directory
rename while the export is active; the rename caller receives access denied
and export continues in the original directory ([Microsoft rename rules](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information)).

Publication requires native hard-link support in the destination filesystem.
Unsupported filesystems report a native error; there is no direct-write or
replacement fallback. In particular Windows NTFS supports hard links, while
exFAT/FAT32 do not ([Microsoft filesystem comparison](https://learn.microsoft.com/en-us/windows/win32/fileio/filesystem-functionality-comparison)).
The staging name uses OS entropy; destination directory access must be trusted.
Windows staging inherits the destination ACL, rather than claiming POSIX mode
bits establish a Windows ACL. A failure before commit never acquires the final
name, so retrying that name does not depend on temporary cleanup. Cleanup only
uses private staging paths and cannot delete the committed target. Cleanup
errors preserve the original result: failure remains failure, and successful
publication remains successful. Private staging remnants can remain if cleanup
is denied or their native identity is unknown; a mkdir collision acquires no
ownership of the existing directory.
Windows paths exceeding the ordinary DOS path limit need explicit extended-length
syntax (`\\?\` and backslash separators) for absolute-path import readers
([Microsoft long-path rules](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)).
Crash durability requires a separate synchronization policy. Hash integrity
does not confer authority;
the target's genuine pause/canonical capture, source/provider/profile and
publication proofs are still required. The file contains no native pointers,
handles, original process epochs or file contents. Limits: 8 MiB wire data,
65536 resource rows and 65536 total FD scan cells per environment, 4096-byte
strings, and 1 MiB total owned text; request quotas are also enforced on import.
The FD scan budget counts live bindings, reserved slots and closed slots
**together**, matching the native debugger and subsequent capture. A sparse
renumbered FD may still have any nonnegative value up to `INT32_MAX`; its value
does not count as that many cells. Oversized tables are rejected before decode
allocations, native file creation or target publication, including later entries
in a group with valid SHA-256 checksums.


## Atomic native environment groups

The DATA-only `llvm_jit_debug_query_wasip1_state_host_api` rejects checkpoint
operations, including `portable_export_group` and `portable_import_group`, with
`invalid_request`. Use the dedicated capture/restore APIs and debugger commands
below; a syntactically valid DATA request does not invoke the group transaction.

`llvm_jit_checkpoint_restore_wasip1_environment_group_host_api` restores
same-process native capsules for 1..16 distinct target environments. All capsules
must be genuine owners from the same nonzero recording label. It prepares every
replacement before publishing any environment and preserves managed aliases
across the supplied environments. Aggregate descriptor quota includes reserved
empty FD cells. A failed preparation changes no target environment.

`llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api` imports
1..16 portable metadata snapshots under one genuine current cooperative stop.
Each request supplies its target `module`, `portable_restore`, matching
`recording_label` and optional `portable_rebindings`. The complete roster must
share a recording label and name distinct actual WASIp1 environments; modules
sharing one environment must submit it once. The target's current mnt and FD
capabilities still authorize every reopened or explicitly rebound resource.
The label is comparison data and cannot authorize restoration or prove that a
Wasm checkpoint was captured at the same stop.

All metadata, quotas, target bindings, source/interface hashes, replacement
files and owned text are prepared before the first environment swap. A failure
reports `request=INDEX` in `diagnostic`, where INDEX is zero based, and leaves
all target environments unchanged. The manager rechecks the actual execution
lease and closed host gate after preparation, for single-environment restores
as well as groups. Once publication starts, it finishes without allocation,
native I/O or a callback, and a subsequent cancellation does not erase the
successful `restored` result.

```cpp
namespace lib = uwvm2::runtime::lib;
namespace pp = uwvm2::uwvm::debugger::wasip1_portable;

// Both files must have been saved with the same recording label.
auto main_state = std::make_shared<pp::snapshot>();
auto provider_state = std::make_shared<pp::snapshot>();
if (!pp::load_file(pp::text_view{u8"/tmp/main.uwp"}, *main_state) ||
    !pp::load_file(pp::text_view{u8"/tmp/provider.uwp"}, *provider_state))
    return;
std::array<lib::llvm_jit_wasip1_environment_capsule_request, 2> imports{};
imports[0].module = main_module_id;
imports[0].recording_label = main_state->recording_label;
imports[0].portable_restore = main_state;
imports[1].module = provider_module_id;
imports[1].recording_label = provider_state->recording_label;
imports[1].portable_restore = provider_state;
// Add portable_rebindings when a saved resource requires an existing target FD.
auto restored = lib::llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
    current_ticket, actual_complete_thread_captures, imports);
```

These group operations are also available through the console and DAP commands
described below. Existing single-environment import commands remain supported.
Portable files preserve alias groups within each snapshot; cross-environment aliasing can be retained by explicitly
binding those resources to the same existing target resource. Independent
mounted-file snapshots do not encode cross-environment open-file identity.
File bytes and already committed external I/O effects remain excluded.
These operations restore WASIp1 resources only; whole-instance Wasm restoration
and atomic joint Wasm/WASIp1 restoration remain unavailable. Always pair the
corresponding Wasm and WASIp1 captures at the same cooperative stop.

Verification of these group APIs, both repositories and all four OS targets is
recorded in [the group test report](../../../../test/0018.debugger/wasip1_checkpoint_group_test_report.md).


### Portable environment groups in console and DAP

The debugger can now capture and import 1–16 distinct WASIp1 environments as one
portable group. Capture obtains one actual complete cooperative cohort and uses a
common random recording label for all environments. Import prepares every target
before publishing any replacement. A refusal such as `request=1 missing rebind`
leaves all environments unchanged. This operation restores WASIp1 state only;
checkpoint Wasm and WASIp1 together at the same cooperative stop.

Console grammar (environment order matches the saved file):

```text
set wasip1 export-group HEXPATH MODULE... [if-stop N]
set wasip1 import-group HEXPATH MODULE[:RESOURCE=TARGET_FD,...]... [if-stop N]
```

For example, `/tmp/group.uwpg` has the UTF-8 hex spelling below. At stop 42,
save environments belonging to modules 0 and 1, then explicitly bind anonymous
resource 2 to target guest FD 3 and FD 7 respectively when importing:

```text
set wasip1 export-group 2f746d702f67726f75702e75777067 0 1 if-stop 42
set wasip1 import-group 2f746d702f67726f75702e75777067 0:2=3 1:2=7 if-stop 42
```

Resource numbers refer to each nested snapshot's resource table. They are not OS
file descriptors. Named resources reopen beneath newly configured target mounts
with matching guest mount names. File bytes remain external. Anonymous files and
external streams need explicit, compatible target bindings; imports preserve
saved alias topology and cannot increase target capabilities.

DAP uses `uwvm/wasip1Edit` with `exportPortableCheckpointGroup` or
`importPortableCheckpointGroup`. Supply exactly one of `path` / `pathHex` and an
ordered `environments` list. Import bindings belong to each list entry:

```json
{
  "command": "uwvm/wasip1Edit",
  "arguments": {
    "operation": "importPortableCheckpointGroup",
    "path": "/tmp/group.uwpg",
    "stopId": 42,
    "environments": [
      {"moduleId": 0, "bindings": [{"resource": 2, "descriptor": 3}]},
      {"moduleId": 1, "bindings": [{"resource": 2, "descriptor": 7}]}
    ]
  }
}
```

The response includes `environmentCount`, `groupAtomic`, `applied`,
`wasmCheckpointRequired`, and a diagnostic naming the failing request where
available. A committed import remains `applied: true` if another controller
resumes afterward; `stopCurrent` reports whether the stop still exists. Invalid
input is rejected before sending, and cached state labels retire before mutation.

Native management entry points:

```cpp
llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(
    actual_current_ticket, actual_before_park_capture_owners, export_requests);
llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
    actual_current_ticket, actual_before_park_capture_owners, import_requests);
```

Each export request selects a target module, sets `portable_metadata_only=true`,
and supplies the same nonzero `recording_label`. Failure returns no partial
snapshots. Two module IDs resolving to the same actual environment are rejected.
A recording label is detached data and does not authorize native restoration or
prove a joint Wasm capture.

The separate version 1 `UWWSGP1\0` group envelope contains bounded version 1
WASIp1 snapshots, per-snapshot SHA-256 checksums, and an envelope SHA-256 checksum.
The group limit is 16 MiB, 65,536 aggregate active/reserved descriptor entries,
and 1 MiB aggregate owned text and mount/path text. A single snapshot still uses
its existing format. All file I/O uses FastIO. Save uses exclusive creation;
existing files are preserved. Single and group exports both stage complete wire
bytes, check payload close and then atomically publish with exclusive hard-link
creation. Write/close/publication failure leaves the target unacquired; failed
staging cleanup may leave a private temporary directory. The final target is
never cleaned up, even when cleanup fails after successful publication. Loading
rejects nonregular files, extra bytes, truncation, mixed labels and unsupported
versions without replacing the caller's output. Cross-OS imports need the same
original Wasm and builtin interface, plus independently configured target mounts
and bindings; native host paths, handles and file contents are never serialized.

Verification of the grouped console/DAP path and directed cross-OS restores is
recorded in [the debugger group test report](../../../../test/0018.debugger/wasip1_checkpoint_debug_group_test_report.md).


### Portable resource text validation

Portable guest mount names and reopen paths must be NUL-free RFC 3629 UTF-8,
including source exports. Single/group encoding and decoding reject lone bytes,
truncated sequences, overlong encodings, surrogates and values above U+10FFFF.
This happens before persistent export or target mount lookup; importing invalid
metadata returns `invalid_portable_snapshot`, even if ordinary target WASI paths
allow raw bytes. Valid non-ASCII mount/path bytes round-trip without normalization.
The target `path_open` UTF-8 check remains enabled as an additional safeguard.
WASI argv/environment remain opaque NUL-free bytes, rather than being subject to
path encoding rules. Existing version 1 files with valid resource text retain
the same encoding.

The original 64 GiB Linux cgroup recovery and post-reboot validation are recorded
in [the portable UTF-8 test report](../../../../test/0018.debugger/wasip1_checkpoint_utf8_reboot_test_report.md).

Native and portable capture apply maximum_descriptors to occupied WASI FD
slots: every live binding (including aliases) and every reserved empty slot
counts once; closed reusable slots do not count. A sparse renumbered FD such as
INT32_MAX still counts as one slot. Native capture checks this limit while
holding the real FD-table lock, before pinning native resources or publishing
a capsule. Rejected captures leave the current environment intact and do not
consume the capsule registry. This request quota is separate from the 65536
total scan-cell bound, which also includes closed cells.

Portable metadata preserves reserved empty FDs in both the dense table and the
sparse renumber map, up to INT32_MAX. Moving the original Rdbg empty FD0 through
guest fd_renumber does not turn it into a native input capability. Import places
a sparse reserved cell in the renumber map, never at a dense vector index. Its
rights and resource-construction state must still match any protected empty
cell already present in the target. Closed free-list entries remain dense.

Portable root-directory restore preserves each resource identity and its follow metadata. Guest path_open(".") descriptors share the authentic configured-root authority but remain separate WASI resources; descriptor aliases alone reuse a resource_index. Only roots issued from the same owned directory entry are equivalent for mount lookup. Distinct configured roots with the same guest name remain ambiguous even when they point at the same host directory. Independently reopened flag variants retain an owned origin reference, which is private process state and is never serialized.

Mounted regular-file provenance canonicalizes relative dot components and repeated interior separators accepted by original WASI path_open. For example ./state.bin and nested//./state.bin persist as state.bin and nested/state.bin. Parent traversal is never lexically folded; absolute paths, trailing separators, Windows alternate separators/streams and empty normalized file paths do not gain portable reopen provenance. The original 4096-byte raw provenance bound still applies before allocation, and lookup follow metadata is preserved. The same FD-manager helper serves wasm32 and wasm64 calls.


Portable FD flags describe the actual native open mode. A native composite
`O_SYNC` mask must be present in full: on Linux, `O_DSYNC` alone exports only
WASI `DSYNC` (2), not `DSYNC|RSYNC|SYNC` (26). Both fdstat ABIs use this rule for
owned files, borrowed observers and directories. Native aliases of sync flags
remain indistinguishable where the host defines identical masks.

A portable DSYNC-only mounted-file restore requires target `PATH_OPEN`, plus
`FD_DATASYNC` or `FD_SYNC`, with sufficient child inheriting rights. It does not
invent a requirement for the stronger `FD_SYNC`. A genuinely saved `SYNC` or
`RSYNC` mode still requires `FD_SYNC`; a host that cannot reopen the requested
mode rejects restoration without silently dropping flags or changing the live
environment. Reconfigure target mounts before importing. This state does not
include pathname contents or reverse external I/O effects. Capture Wasm and
WASIp1 together at the same cooperative stop.


Previously persisted snapshots may already contain the erroneous value 26.
That value is also valid for a genuinely synchronous native file, so import
must not blindly rewrite it to 2. Recapture from the original live environment
with the corrected implementation when a DSYNC-only recording needs repair.

Original WASIp1 fd_fdstat_set_flags replaces the complete native synchronization mask when selecting DSYNC. An existing SYNC open cannot silently retain its stronger flags and return success for a DSYNC request. If the OS cannot change that open description, ENOTSUP is returned and accompanying APPEND/NONBLOCK changes are rolled back. Owned and borrowed files/directories and their shared aliases follow the same rule; wasm32 and wasm64 use the shared setter. See [the synchronization transition report](../../../../test/0018.debugger/wasip1_checkpoint_sync_transition_test_report.md).

Directory path_open with a terminal dot or parent component issues an independent FastIO native description with the requested supported flags, retaining the configured mount owner. Editing that child cannot change its preopen. Portable restore also reconstructs distinct root resources independently, including equal-flag resources; descriptor aliases still share one saved resource. A changed root mode may use existing mutable SET_FLAGS authority or one authorized PATH_OPEN parent with all inherited child capabilities and synchronization rights. It does not require SET_FLAGS on a parent when constructing a fresh permitted directory. Windows directory NONBLOCK is explicitly unsupported. See [the directory flag test report](../../../../test/0018.debugger/wasip1_checkpoint_directory_flags_test_report.md).

Fresh root resources derive both child base and inheriting rights from one PATH_OPEN parent's inheriting rights, just like the original path_open. The parent does not need to hold those child rights in its own base set. A preexisting matching root can instead authorize reconstruction through its held rights. Closing a target child before import cannot turn a legitimate delegated reconstruction into capability_denied; stronger synchronization still requires its own parent authority.

Portable root lookup checks the saved rights and native flags together on ONE actual target FD. A lower-numbered root with different flags cannot hide a later complete matching or independently mutable root. Separate roots cannot combine PATH_OPEN, inheriting rights or SET_FLAGS to manufacture restoration authority. Native observations and positive/negative selections are cached only within the current closed-gate import, then discarded. See [the root selection regression report](../../../../test/0018.debugger/wasip1_checkpoint_root_selection_test_report.md).

### 新 world 的私有间接调用绑定（R36）

候选 world 在旧 worker 退休前准备 caller-local 类型 canonical 表和 LLVM call_indirect 表视图。表导入的每一跳、表元素的函数指针和最终目标都必须属于新 source 的实际记录；defined 目标指向新 LLVM 引擎，import 目标借用新私有 import cache。Core 3.0 子类型通过未缓存的完整类型匹配投影到调用者类型空间；无匹配的有效引用保留不匹配 sentinel，null 和 GC/extern/exn 表保留原语义。准备过程不修改当前 runtime 的表刷新 hook 或 TLS 类型缓存。

maximum_private_indirect_bindings 默认 1,048,576，合计限制类型记录、所有 caller 的表视图和 function-table 目标槽；import alias 的目标槽按每个 caller 单独计数。超限返回 indirect_binding_preparation_declined，旧 guest 保持同一 pause 和 epoch。结果的 runtime_indirect_bindings_prepared 和目标分类计数只是私有准备诊断，不能执行 guest，也不表示 whole-world publication、恢复线程启动或恢复重放已完成。

Wasm checkpoint 检测到 WASIp1 时仍必须在同一个停止点同时 checkpoint WASIp1；文件内容和外部 I/O 副作用不属于状态回滚。

### 候选引擎原生端点捕获（R37）

启用 NativeOwnerTable V2 的 checkpoint 候选编译会在真实 LLVM 装载前安装私有端点观察器。typed Wasm 函数和同一 sealed plan 的 resume 函数保留 role 1/2 期望；raw adapter 和其他 helper 只保留 role 3/0 冲突记录。完整 object/symbol ledger、真实端点、别名、重叠与 engine 实际解析入口必须全部一致，观察器才冻结私有 DATA 并脱离 engine。

冻结不会设置 live native-owner seal，epoch 仍为零，也不会发布 ASM lookup、断点或恢复权限。ASM 调试范围仍只允许真实 Wasm 生成的上下文，不能用这个接口调试 VM 或 host helper。整体 world 发布必须在真实旧线程 drain/join 后另行完成实际 source/engine/GC/WASI/worker 归属交接。

maximum_private_native_endpoint_functions 默认 65,536，合计 typed/resume 期望并在生成候选 engine 前拒绝超限。object 解码和完整 symbol claim 的私有 backing 计入原 maximum_native_payload_bytes；配额或端点观察失败不会退休旧 worker。结果的 runtime_native_endpoint_capture_prepared 和计数只有诊断作用。未启用 NativeOwnerTable V2 时不宣称捕获端点。

Wasm checkpoint 检测到 WASIp1 时仍须同一次 stop 中同时 checkpoint WASIp1；文件内容和外部 I/O 不属于状态回滚。


候选整体 world 还会预先准备 main/preload 的 source initializer 对应表：每个记录必须匹配新 source 实际 parser 文件、不可移动 registry 成员及真实 dense 顺序，GC staging 资源全部填充且保持未发布。maximum_private_source_initializer_modules 默认 4096，并在创建新 engine 前拒绝超额；内存由 maximum_native_payload_bytes 计费。真实旧 guest drain 和 OS/TLS join 完成后，管理器再次检查该表、旧 world 的当前 source/epoch/initializer serial 和实际 builtin WASIp1 loader 归属，才报告 prepared_world_ready_closed。两个 source/epoch 的完整发布与恢复线程启动仍是独立未完成步骤；这里没有发放新 initializer serial、source seal、执行权限或 VM/host ASM 调试上下文。Wasm 与 WASIp1 仍须在同一停止点一起 checkpoint。


R38 native object quota diagnostics: the private actual-object/symbol ledger
reserves 16 times the loaded object byte count, plus individual symbol storage,
before decoding. The public native payload default stays 256 MiB. Large modules
with thousands of instruction resume landings may need an explicitly larger
request budget; the complete instance test uses 512 MiB and the two-module
preload test uses 768 MiB. Both continue to exercise smaller budget refusal. A loader quota refusal now reports the actual
resource phase (quota_exceeded, diagnostic 9) and engine exhausted (diagnostic
5), while malformed endpoint declarations, observer setup and freeze checks
retain distinct engine diagnostics (7, 8 and 9). These are private preparation
results; no candidate is published and the original paused world stays intact.


### Private final publication records

Before retiring the current native cohort, the manager allocates the final full-runtime record shape for every candidate module: typed/raw/resume entries, function generations, safe-point views, empty compiler-metadata slots and retained-generation capacity. maximum_private_full_publication_functions defaults to 65536 and is charged to the existing native payload budget. Exceeding it reports full_publication_record_preparation_declined and leaves the original pause, source epoch and initializer serial unchanged. The same real records and staged engines are rechecked after execution drain and physical OS/TLS join.

This stage preserves the exact typed-target buffer already embedded in LLVM IR; joint commit must move that allocation and the actual engine/metadata owners. Reserving retained-generation capacity inserts no null generations. These private records own no executable engine, LIVE native seal, runtime epoch or ready flag. Joint source/GC/WASIp1 publication, restored-worker startup and replay remain unfinished. Checkpoint Wasm and WASIp1 together at the same actual stop; external file contents and I/O effects are not rolled back.
