WASIp1 debugger environment contract
===================================

This component adds management-only queries and edits alongside the Wasm state
manager. It preserves the original WASIp1 import behavior, strings, path handling
and syscall adapters. The resource RC adds runtime-only managed identity metadata.
Disabled call
tracing adds one atomic flag check at the builtin import wrapper.

The console accepts:

```
info wasip1 args MODULE [FIRST COUNT]
info wasip1 env MODULE [FIRST COUNT]
info wasip1 fds MODULE [FIRST_GUEST_FD COUNT]
info wasip1 preopens MODULE [FIRST_GUEST_FD COUNT]
set wasip1 arg MODULE INDEX HEX_BYTES
set wasip1 arg-insert MODULE INDEX HEX_BYTES
unset wasip1 arg MODULE INDEX
set wasip1 env MODULE HEX_KEY HEX_VALUE
unset wasip1 env MODULE HEX_KEY
set wasip1 rights MODULE GUEST_FD EXPECTED_BASE EXPECTED_INHERITING NEW_BASE NEW_INHERITING
```

The `-` byte token means empty text. Other byte tokens contain exactly two hex
characters per byte. NUL is rejected; environment keys cannot contain `=`. The
existing environment UTF-8 policy remains unchanged. Display escapes non-graphic
bytes, quotes and backslashes; guest text cannot emit terminal control sequences.
Decimal and hex parsing and output use FastIO. Rights masks accept unsigned decimal or `0x`/`0X` hex, including masks copied
from `info wasip1 fds`. The complete console and authenticated broker accept
8448 input bytes for WASIp1 text editing, enough for a 4096-byte argument or
4096-byte environment entry. Other command limits remain in place.
`arg-insert` inserts before INDEX; INDEX equal to current argc appends.
`unset wasip1 arg` removes that index and shifts subsequent arguments.
Both operations build complete backing/views and enforce the same pause,
ownership, entry and byte budgets before the existing atomic buffer commit.

DAP `uwvm/wasip1Edit` accepts `operation` (`replaceArgument`, `insertArgument`,
`removeArgument`, `setEnvironment`, `removeEnvironment`, `reduceRights`),
`moduleId` and optional expected `stopId`. Text uses `value`/`name` (UTF-8) or
`valueHex`/`nameHex` (opaque bytes), exactly one spelling per field. Arguments use
`index`; rights use `descriptor`, `expectedBase`, `expectedInheriting`, `newBase`,
`newInheriting` as unsigned integers. The reply reports `status`, `applied`,
`stopCurrent` and `sharedEnvironment`; a committed edit remains `applied:true`
if another host subsequently resumes. Inspect state before retrying an ambiguous
transport failure. Direct edits and scripts retire copied DAP views before send.
Queries also expose an explicit `available` flag. DAP sends every edit with
`if-stop STOP_ID`; the controller checks it under its lock before environment
admission. Console mutations can append the same optional selector.

`MODULE` selects an actual published runtime module, whose actual configured
WASIp1 environment is resolved by the existing VM resolver. A shared environment
is explicitly labelled; an edit changes that same environment for every module
which shares it. No native address or descriptor, host path, raw OS file type,
callback, clock override or filesystem handle is exposed.

Admission is the existing coherent manager's ONE synchronous proof scope:
actual generation lease, canonical before-park captures for the entire current
cooperative roster, same immutable profile/control and pause ticket, nonwaiting
actual host gate closure, complete owned GC admission, then actual publication.
The private adapter has no public constructor or callable entry; only the manager
is its friend. Native trap stops cannot reuse an earlier cooperative episode.
No supplied boolean, pointer, reader count, stop number, snapshot file or display
label substitutes for that closure. Active/untracked plugin or host activity is
refused without touching live state. The manager rechecks its real stop atomic
before dispatching the adapter. A committed edit remains reported as applied if a
trusted stop is requested immediately afterward.

Arguments and environment queries prove each existing borrowed view has precisely
the corresponding owned string's data address and size. Edits build complete new
backing and view vectors before committing two noexcept buffer swaps. No guest,
host provider or allocator callback runs between the swaps. Retired storage dies
after both swaps. The original configured global or group backing retains
ownership until the existing initializer next replaces it; reset never leaves a
new runtime-only backing dangling. Duplicate matching environment keys and
unowned views fail without mutation. Text vectors are limited to 4096 entries and
1 MiB including terminators; pages contain at most 64 strings of at most 4096 bytes
each, with a 4096-byte total raw-text budget. Page iteration stops before the
first row that would exceed that budget and reports the exact next index. The
formatter also checks detached result bounds before printing; even fully escaped
hostile text plus all row labels remains below the actual 65536-byte broker cap.

FD metadata follows the original table-lock then descriptor-lock order. It copies
only guest FD number, storage kind, two rights masks, guest preopen name and a
diagnostic managed resource identity.
`file` denotes storage kind, not an invented OS regular-file type. The original
prestat directory chain supplies the guest preopen name. It performs no `fstat`,
open, close, dup, seek, renumber, socket or filesystem syscall. At most 65536 slots
are scanned per request. Guest FD identifiers from the actual vector and the
renumber map are collected, bounds/duplicates checked and sorted before paging;
the map's iteration order is never assumed. Page count and raw-text limits close
the page at the first omitted eligible row, so `next` cannot skip a lower FD.
Rights mutation first compares both actual old masks,
then proves both new masks are subsets, and only then writes either mask.

The independent masks follow the official Preview1 contract: rights can be
removed, not increased. The normative reference is the preserved official
[Preview1 specification](https://github.com/WebAssembly/WASI/blob/a2b96e81c0586125cc4dc79a5be0b78d9a059925/legacy/preview1/docs.md#-fd_fdstat_set_rightsfd-fd-fs_rights_base-rights-fs_rights_inheriting-rights---result-errno).
Arguments and environment are the environment consumed by subsequent WASIp1
imports. Already copied guest-memory `argv` or libc-owned environment is not
silently rewritten; it remains independently inspectable Wasm memory.

Resource construction and live checkpoint restoration are available through the
managed provider described in [wasip1_checkpoint.md](wasip1_checkpoint.md).
External I/O replay and native register/memory access remain unavailable. A guest `proc_raise`
signal must never mint a debugger keyboard-interrupt request. Any future WASIp1
signal catchpoint must originate in authenticated generated host dispatch, rather
than inferring debug authority from an OS signal or a numeric PID.

Tests provided as source: a standalone parser/formatter/rights contract component,
plus a real Core3 GC/LLVM-full program with actual WASIp1 imports. The runtime
fixture exercises genuine before-park capture and complete management admission,
args/env ownership edits, FD masks, stale rights, attempted escalation, a forged
same-address shared owner, and refusal after resume. The original imports must
read the edited byte counts and bytes and return 91. Both independent stack
strategies and both products must be built and executed in the Linux keeper's
64 GiB cgroup. The integration runner is
`python3 test/0018.debugger/test_wasip1_live.py UWVM WASM_TOOLS OUTPUT_DIRECTORY`;
it enforces cgroup admission and exercises the actual CLI, authenticated socket
broker, DAP edits, pause selectors, long text, filters and a real EBADF return.

Dynamic ABI trace: `trace wasip1 on [NAME|all]`, `off`, `clear`, and
`read [AFTER COUNT]` (count 1..64). It covers the original builtin WASIp1
wrappers, including configured wasm64/socket variants. Entry rows carry raw
unsigned i32/i64 argument bits; return rows carry Preview1 errno code and name.
They never dereference guest pointer offsets or enter host/VM assembly debugging.
Entry and return share a monotonically unique `call` label. A trap/nonreturning
`proc_exit` has only an entry. Turning tracing off stops new entries but lets
already observed calls report returns; clear retires outstanding pairs. The
bounded journal retains 512 rows, reports overwritten entries/cursor gaps, and
is process-wide and can be read while running; the local console also allows
reads after guest exit. External VM channels close at guest exit, so retrieve
those logs while the guest is still alive. Names/sequence labels grant no
pause or guest-memory capability. This trace does not implement syscall entry
catchpoints, arbitrary host-call replay, pointer-structure decoding or external
I/O checkpoint restoration.
