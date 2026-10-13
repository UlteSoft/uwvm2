# R45 — stopped guest memory pages through DAP

Both `uwvm2` and `uwvm2-ros` support DAP `readMemory` for an actual guest
linear-memory selector. One request accepts **0..65536 bytes**; the adapter
copies consecutive pages of at most **256 bytes** through the existing
FastIO console formatter. Memory32, memory64 and multi-memory keep their
actual module and memory index spaces. No native pointer is accepted.

Example request (memory 1, byte offset 17 after signed displacement):

```json
{"seq":1,"type":"request","command":"readMemory","arguments":{"memoryReference":"wasm-memory:0:1:64","offset":-47,"count":4096}}
```

The successful body has numeric guest offset `"address":"17"`, base64 `data`
for exactly 4096 bytes, and `"unreadableBytes":0`. `memoryReference` belongs
in the request; the response address follows the DAP numeric decimal/hex
address contract ([official schema](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json)).
The data is guest linear memory, not a host address or an ASM memory capability.

## Stop and range checks

A new logical selector refreshes the actual current status. It requires a
nonempty, fully identified cooperative stopped cohort. Native traps, legacy
unidentified stops, running/exited guests and native/code/stack references
are refused before the memory copy.

Every page is surrounded by fresh status checks against the original complete
stop identity: reason, cohort, participant, module/function/byte offset,
publication generation, native state and nonzero stop ID. Every reply must
have exactly the requested number of two-digit bytes and one final newline.
A stop change, malformed/short reply, transport loss or unavailable guest
range fails the **whole request**, without publishing the copied prefix.
Failure also retires cached frame/value/code/scope references.

Zero `count` returns empty base64 data only after a real runtime range check.
An offset exactly at the guest extent is valid for zero bytes; an offset past
that extent is refused. Module IDs retain uint64 bounds, memory IDs uint32,
and guest offsets/range ends use the existing native uint64 wire bounds.
Booleans, fractional/string counts, count above 65536 and overflow/negative
normalized ranges fail before broker I/O.

This is a stopped display operation. It does not implement an atomic memory
checkpoint, snapshot replay or external I/O rollback. Runtime reset,
publication and debug administration retain their existing serialization
contract; independent writes that preserve the entire stop identity do not
create a new checkpoint epoch here.

## Verification

The new unit file checks boundaries, signed offsets, uint64 addresses, strict
reply shape, initial and final stop identity, publication/cohort changes,
transport loss, failed later pages, cache retirement, zero-byte runtime bounds
and mixed/native cohorts. Protocol models are separate from native evidence.

`run_dap_wasm_memory_pages.py` sends real DAP frames through `Adapter.handle`
and a genuine authenticated Unix broker. The actual LLVM JIT fixture has two
128 KiB memories: memory32 filled with `0x51`, memory64 filled with `0xa7`.
Both repositories run instruction and unwind stack policies. Each case checks:

- Twenty successful reads, including 64 KiB reads of both memories, negative
  displacement, 256-byte page boundaries and zero-byte reads at the extent.
- Eight genuine unavailable/overflowing guest ranges, including a second-page
  failure after a successful first copy; no prefix escapes.
- A real guest-memory commit `ff aa bb cc` followed by a real Wasm step during
  a four-byte copy. The response fails even though the copied old bytes were
  valid at the earlier stop. Three other actual first/middle/final-page steps
  also fail, and an actual ASM trap refuses without issuing `memory`.
- Fresh cooperative reads after native stepping and original guest natural
  exit 0, with pinned inputs and original broker/guest cleanup.

The frozen previous adapter is checked using that same four-byte commit/step
case. It incorrectly returns `UVFRUQ==` (`51 51 51 51`) after actual guest
memory has become `ff aa bb cc` and the stop ID has advanced. The corrected
adapter withholds this stale result.

R45 acceptance: **324 unit tests per repository, zero skips**, four new memory
native cases plus **30 native regressions** (WASIp1 scopes, seven typed scopes,
GC members, local pages, frame lifetime, queued startup and ASM finish/timeout).
These run only on SSH Linux x86_64 in the original shared **64 GiB** cgroup,
with swap disabled, shared admission lock, pinned executables and original
Popen/PIDFD retirement. No foreign task is adopted, killed or cleaned.

Only the Python adapter and its tests/docs change in R45. Native tests reuse
the qualified R37 full interpreter/full LLVM products and the single existing
SDK; they do not claim a new build of concurrently edited C++. Source/SDK pins
and eight compiler dependency closures are reverified. This does not qualify
Windows, macOS, FreeBSD, AArch64, musl or an IDE UI on this Linux-only run.
The Python paging logic introduces no platform-specific native memory reader.

The final resource receipt and primary broker/DAP records are in
`/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r45.json`
and `primary-evidence-r45.tar.gz`; the retained local verified copy is under
`/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r45-memory-pages/`.
