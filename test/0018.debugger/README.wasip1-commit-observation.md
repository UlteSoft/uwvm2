# R47 — checkpoint result survives a failed observation

The adapter previously validated a genuine successful WASIp1 operation ACK,
then allowed failure of its follow-up `status` request to erase that known
outcome. A successfully saved or restored capsule could be reported as a DAP
failure, inviting a duplicate operation. Both repositories now preserve the
complete acknowledged result separately from the later observation.

```json
{"seq":1,"type":"request","command":"uwvm/wasip1Edit","arguments":{"moduleId":0,"operation":"saveCheckpoint","slot":2}}
```

Restore with `operation:"restoreCheckpoint"`, the same slot and explicit
`resourceMode:"bindings"` or `"strict"`. Drop with `operation:"dropCheckpoint"`.
These commands retain the existing genuine current cooperative stop and
`if-stop` admission; no snapshot DATA or ASM label supplies restore authority.

After a complete valid ACK, failure or malformed output from the subsequent
status observation cannot change `applied`, `available`, recorded epoch,
resource counts, slot, affected FD or portable metadata. The response carries
`stopCurrent:false`, `stopObservationAvailable:false` and a bounded diagnostic
that the operation outcome is confirmed while the stop needs refreshing.
An ordinary valid refresh carries `stopObservationAvailable:true`; independently
resumed or different stops still have `stopCurrent:false`.

Old frame, scope, value, source and native code labels retire before every edit
attempt. Lost operation acknowledgements remain unconfirmed, with no retry or
follow-up status that guesses the operation outcome. Malformed operation ACKs
still fail; observation failure cannot relax metadata validation. Negotiated
`invalidated` events follow confirmed applied responses, including those with
an unavailable observation. Refusals and unconfirmed edits emit no mutation
event. Raw transport errors do not enter the observation diagnostic.

The protocol tests distinguish all eight checkpoint slots, ordinary resource
edits, portable/group operations, complete acknowledgements, nonapplied
refusals, lost replies, strict observation shape/cohort bounds, and negotiated
event order. They are DATA doubles, not execution or restore proof.

The actual Linux driver creates and aliases a managed anonymous binary file,
saves a real capsule, modifies its bytes/cursor/environment and removes its
guest descriptor bindings. Actual restore recreates the bindings and saved
rights, file bytes/cursor and environment; guest `fd_pread`, `fd_pwrite`,
`fd_seek` and `fd_tell` probe those effects. A shared alias cursor is checked.
The driver closes the actual authenticated channel only after receiving the
genuine operation ACK, so the subsequent status fails. This is explicit
adapter/channel fault injection, not a kernel or network-failure claim.
It separately withholds a genuine committed ACK, verifies the effect after
reconnecting without replay, tests a known edit followed by actual independent
resume, and rejects checkpoint admission at a real ASM trap.

The baseline uses the frozen previous adapter and the same native fixture:
it reports failure after the genuine save ACK, but a real restore proves that
the supposedly failed save created a usable capsule.

Native qualification rebuilds both full-interpreter/full-LLVM CLI consumers
using the exact current checkpoint branch from both repositories. All other
C++ source/provider inputs retain the pinned R37 cut, with unchanged runtime
and host-api objects and one existing SDK. Each compiler dependency closure
is verified; this is not a build of all concurrently edited C++. Tests execute
in the original SSH Linux x86_64 64 GiB/swap0 cgroup. They do not qualify an
IDE UI or claim another platform. The original shared admission lock and Popen/PIDFD ownership guard
remain mandatory; foreign processes and products are preserved.

This repairs WASIp1 checkpoint operation delivery. It does not add whole-Wasm
instance/continuation restore or external filesystem/network/output rollback.
The actual WASIp1-only restore deliberately leaves a changed Wasm memory byte
untouched. `bindings` retains external resource identity; `strict` rejects
external resources before commit. Native C++ parsing/printing and existing
managed storage continue to use FastIO.


## Executed verification and retained attempts

The final adapter/tests execute 354 unit methods per repository with zero
skips. Four new actual checkpoint cases and 38 actual regressions run on the
two rebuilt CLI consumers. The new cases include 16 failures of post-ACK
status observation: 12 confirmed saves/restores/drops and four confirmed
strict nonapplied refusals. They reject 140 retired references. Four further
host-side injections withhold a genuine committed create-file ACK, proving
exactly one new guest descriptor after reconnect and no replay.

The old adapter's actual saved-capsule/failure baseline is retained separately.
The initial v1 driver attempted a debug memory write that the runtime correctly
refused for this imported host state. V2 wrote decimal 170 through a hex bits
grammar and obtained the wrong marker. V3 corrected the driver and reached
a real old-CLI drop ACK with epoch=0; the adapter correctly rejected it.
That old CLI lacked the already-present current controller metadata fix.
V4 rebuilds the exact current checkpoint branch, preserving nonzero epoch
validation. All raw failed attempts, immutable input cuts and original broker
retirement receipts remain evidence; failures are not counted as passes.

A local freeze also encountered ENOSPC. Only this task's six previously
archived R46 snapshot directories and their compressed duplicate uploads were
removed, after archive/member hashes and process references were checked.
The source, preimages, primary evidence archives, records and foreign files
remain. Its partial unpublished R47 freeze was removed before retrying.
Remote source overlays hard-link unchanged immutable inputs and do not copy
the SDK. Resource accounting is included in the final receipt.

Final delivery detected concurrent language edits adding unparenthesized
sizeof expressions, reserved-keyword checks and corpus cases. These are
preserved in the exact V5 input cut. Both unit suites and all 42 native cases
are rerun on that merged cut; the WASIp1 edit/response paths and rebuilt
checkpoint branch are byte-for-byte unchanged. No foreign files are reverted.

Primary qualification is
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r47-v5.json;
the member-hashed primary archive is primary-evidence-r47-v5.tar.gz.
The verified local copy and delivery proof are retained under
/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r47-checkpoint-commit/.
