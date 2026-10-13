# R46 — bounded guest memory writes through DAP

Both repositories expose `supportsWriteMemoryRequest` and accept a standard
DAP `writeMemory` request for **guest linear memory only**. One nonempty
request owns 1..256 bytes and maps to one existing native
`set wasm memory MODULE MEMORY PARTICIPANT OFFSET bytes HEX` commit.
The C++ parser and formatter use FastIO; the runtime uses
`fast_io::freestanding::my_memcpy` only after checking the entire actual
writable extent inside its current cooperative transaction.

```json
{"seq":1,"type":"request","command":"writeMemory","arguments":{"memoryReference":"wasm-memory:0:1:64","offset":-47,"allowPartial":false,"data":"AAF//w=="}}
```

This writes `00 01 7f ff` to memory 1 at guest byte offset 17. The successful
body is `{"offset":-47,"bytesWritten":4}`. Memory32, memory64 and multiple
memories retain their actual module/memory indices; offsets are signed and
bounds-checked. Data must be canonical ASCII base64 of 0..256 bytes.
`allowPartial=true`, larger payloads, malformed base64, noninteger/boolean
offsets, unsigned overflow and host/code/stack references fail before I/O.
This explicitly implements the full-write subset of the
[official DAP write/memory-event schema](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json).

Empty data is a no-op only after a genuine zero-byte runtime range read. It
may target the guest extent but cannot target past the extent. It emits no
memory event and does not advance the stop or retire valid cached views.

## Commit and reference lifetime

The adapter first requires a current identified cooperative stopped cohort:
1..256 distinct nonzero uint64 participants, nonzero bounded generations,
bounded module/function/offset fields and no native PC. R45 guest reads now
use the same stricter cohort admission. The native controller and runtime
repeat their genuine current pause/cohort/owner/type/extent proof before the
single complete copy. Invalid ranges never receive a prefix write.

The selector is logical guest DATA. It acquires the actual current native
transaction when executed; it is not an old-frame capability or an optimistic
`if-stop` write. It carries no native pointer, SP/FP, code address, host handle
or VM memory authority. At an ASM trap the adapter refuses before the write,
and the native mutation API independently refuses native-owned stops.

Before a nonempty attempt, all copied frame, value, scope, code and deep-path
references retire. Only an exact v3 acknowledgement of the requested
module/memory/offset, full byte count, available status, applied commit,
nonzero runtime epoch, actual address width and advanced stop ID confirms
success. A genuine nonapplied refusal fails with its reason. A malformed or
lost acknowledgement is **unconfirmed**, with a diagnostic to inspect memory
before retrying; the adapter never replays or splits the write.

Once a full commit is acknowledged, a later status/read failure or independent
resume cannot erase its successful `bytesWritten` response. No fragile
post-commit status request is used as a success condition. Views remain
retired until freshly queried. When the client initializes with
`supportsMemoryEvent` and/or `supportsInvalidatedEvent`, successful nonempty
writes send the corresponding memory-range and stacks/variables refresh
notifications after the response. Unnegotiated, refused and zero-byte writes
send no such events.

## Verification and qualification scope

Seventeen new unit methods exercise binary sizes through 256 bytes, canonical
base64, signed offsets and uint64 selectors, strict cohort bounds, exact
acknowledgements, genuine nonapplied packet shapes, whole-range refusal,
pre-write retirement, lost acknowledgement without replay, successful commit
despite later status loss and event negotiation/order. These are protocol DATA
checks, separate from native qualification.

`run_dap_wasm_memory_writes.py` dispatches genuine DAP frames through
`Adapter.handle` and the real authenticated broker. Its actual LLVM fixture
is the R45 validated Wasm 3.0 module with two 128 KiB memories (memory32 and
memory64). For both repositories and both instruction/unwind stack policies,
each actual case verifies:

- Twelve binary writes of 1, 2, 3, 128, 255 and 256 bytes to both memories,
  exact native bytes/widths, one commit only, pre-commit cache retirement,
  response-before-event ordering and 36 old frame/local/source-label refusals.
- Writes to the final guest byte, zero-byte extent checks, ten genuine range
  or module/memory refusals, and unchanged bytes/stop IDs after rejected full
  ranges, including a 256-byte request with only 72 valid bytes remaining.
- Running-state refusal, eight invalid requests with no broker I/O, actual ASM
  trap refusal, and rejection of its actual opaque native code label and PC.
- A real acknowledged commit followed by an independent real `continue`
  before the adapter sees the reply. It remains successful; its bytes are
  checked after reacquiring a cooperative stop.
- A host-side fault that closes the actual authenticated channel **after the
  genuine committed reply was received** and withholds it from the adapter.
  The response is unconfirmed, with no replay. A newly authenticated channel
  proves the actual committed bytes, three old references remain retired, and
  a fresh write/read recovers. This is explicit adapter reply-loss injection,
  not a claim of a kernel/network failure test.

The frozen R45 adapter fails the same actual four-byte write request as
unavailable, and actual guest memory stays `51 51 51 51`; this is the native
baseline, not a mocked VM reply.

R46: **341 unit tests per repository, zero skips**; four new write cases plus
**34 native regressions**, covering R45 memory reads, WASIp1 scopes, typed
scopes, GC/local pagination, frame lifetime, queued startup and eight ASM
finish/timeout cases. All execute on SSH Linux x86_64 in the original shared
**64 GiB/swap0** cgroup with the shared admission lock and original Popen/PIDFD
ownership/retirement. No foreign task or product is adopted, signaled or deleted.

The final v3 snapshot preserves another agent's concurrent source-expression
parser and corpus/test updates in both repositories. The unit and native
suites are rerun on that merged snapshot; the new memory-write implementation
is retained unchanged. Earlier successful receipts and the initial driver
frame-selection failure remain historical evidence.

R46 changes the Python adapter and tests/docs. Native cases reuse qualified
R37 full-interpreter/full-LLVM products and the one existing SDK, rechecking
source/SDK pins and eight compiler dependency closures; this is not a new
build of concurrently edited C++. Windows/macOS/FreeBSD/AArch64/musl and IDE UI
are outside this Linux native acceptance. This operation does not add complete
instance checkpoint restore or external I/O rollback.

Primary receipts are retained in
`/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r46-v3.json`
and `primary-evidence-r46-v3.tar.gz`, with the verified local copy under
`/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r46-memory-writes/`.
