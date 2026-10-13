Runtime-private physical Wasm backtrace
======================================

`llvm_jit_debug_native_backtrace_host_api` accepts the canonical native
activation cursor, its active session and a frame limit from 1 to 32. It returns
only individually authenticated Wasm parent identities, nearest first: module,
function, function generation, runtime epoch and distinct dynamic incarnations.
No native return address, CFA, SP/FP, saved register or stack byte is returned.
The legacy single-caller API uses the same transaction with a one-frame limit.
These are inspection data and cannot resume execution.

At an actual native stop, `bt THREAD` displays up to 32 physical Wasm parents.
`complete to Wasm root` means every parent up to the actual logical Wasm root
was proved. Unsupported CFI, a different physical/logical parent or the frame
limit yields an explicitly partial proved prefix. With no proved parent it
reports `physical Wasm caller unavailable`. Stale stop identifiers are refused
before reading. Formatting rejects oversized or disconnected frame data.

The query pins the actual current or committed replacement engine, execution
lease, original pause-domain ticket, publication and native trap gate through
all reads. Each frame selects that exact engine's registered CFI and requires a
return address inside the actual parent function/generation/epoch. Forward MC
decoding from that parent's exact entry must end immediately after a supported
returning call for the qualified target. No host return address is decoded
and no host frame is walked.

The actual debug worker registers its pthread stack bounds and TID during cold
execution-scope entry. This metadata retires after native worker ACK and does
not keep the departed OS stack allocation alive. Starting from the real kernel
trap, each level recovers only CFI-proved preserved registers and CFA/SP.
Unavailable volatile registers remain unknown; they cannot supply a later CFA
or register-memory address. SP cannot decrease between physical frames.
LoongArch permits an actual frameless C ABI return at an unchanged SP only
with independently authenticated parent/code/CFI evidence; an identical
PC/SP/body cannot nominate another physical activation.
Only designated CFI slots inside SP..CFA are copied at the actual target ABI
width: four bytes on i386 and eight on the qualified LP64 targets,
into a bounded target-specific preserved-register array per frame: seven
words on SysV x86-64, five on i386, thirteen on RV64, twelve on AArch64 and
eleven on LoongArch64 (each includes its return-address column). The frame extent is checked against
the actual registered worker stack, without a fixed 64 KiB frame cutoff. The
sparse owned scratch is cleared at every level.
FastIO's Linux syscall interface performs each bounded kernel copy, refusing
unmapped/protected slots without a manager fault. Reaching the Wasm root stops
before reading its return slot, saved host registers or host caller.

The first backend supports SysV Linux x86-64 integer CFA rules and a return
address at CFA-8. Later independent records qualify
[RV64](native_finish_riscv64.qualification.json),
[AArch64 LE LP64](native_finish_aarch64.qualification.json) and
[i386](native_finish_i386.qualification.json) and
[LoongArch64 LE LP64](native_finish_loongarch64.qualification.json).
See [LoongArch usage and required backend repairs](README.debug-loongarch64-finish-status.md).
i386 uses CFA-4 and actual
RET32/RETI32 return adjustment. Other targets remain unavailable through the
physical-caller/finish API. Expressions, unsupported CFI/targets and frames
outside the registered worker stack cannot supply guessed native context.

`debug_native_physical_caller_runtime.cc` uses the ordinary owning initializer,
the selected full-runtime/LLVM profile, actual generated recursive Wasm calls,
real kernel stepping and worker ACK. Shallow and deep scenarios require a
complete multi-frame chain, an explicit 32-frame prefix for deep recursion,
validated generation-two inactive-leaf replacement callers, distinct recursive
incarnations, one-frame limits, zero/oversized-limit refusal and stale/alias/
missing-domain refusal. Official wasm-tools parses and validates the WAT.
`debug_native_physical_caller_console.cc` reaches deeper real recursive stops
and checks multi-frame `bt`, FastIO formatting, malformed display refusal,
stale-stop retention and genuine cancellation/ACK. Refusals alone cannot
satisfy positive runtime coverage.

`llvm_jit_debug_mint_native_return_continuation_host_api` now creates a
runtime-private, immutable physical-return proof from that same genuine first
parent transaction. It retains the canonical originating cursor and privately
binds the actual kernel trap revision, thread, origin PC/SP, decoded parent
return PC/CFA, both exact code owners, epoch/generation and dynamic incarnations.
No raw address or stack/register field appears in its public query. The bounded
weak registry compares both pointer and control-block identity before reading
any supplied alias; live entries are never evicted to accept another proof.

`llvm_jit_debug_native_return_continuation_host_api` re-reads the actual CFI,
owned stack, parent code and kernel trap under the original execution/domain/
publication/native gate and compares all private evidence. It returns only the
authenticated Wasm parent identity. Cooperative captures, root/host returns,
wrong sessions, unreadable aliases, same-pointer different-control-block
counterfeits, missing park ownership, later traps, native ACK and runtime reset
cannot supply a current proof. A retained stack-registration label does not pin
a departed OS stack. Neither API accepts a target address or copied native data.

Backtrace inspection and the protected proof callback below do not themselves
install an executable return event. [Native finish](README.debug-native-finish.md)
separately joins a sealed return-stop transaction, parent activation publication,
generation ownership and actual event/cancellation/ACK lifetime. This backtrace
does not implement arbitrary native stack memory display or full
instance/external I/O rollback.

Qualified native run on 2026-10-06
----------------------------------

Both ordinary uwvm2 and uwvm2-ros passed 71 selected build, runtime and
regression commands in the original SSH Linux 64 GiB cgroup. The fresh product
TUs and fixtures use the full interpreter and actual LLVM 23.1.1-uwvm-ros.11
SDK, with all 13,044 SDK inputs and frozen source hashes checked afterward.
The first compile failure from an unwrapped FastIO status string is retained
and excluded; the repaired source was freshly compiled in both repositories.
The selected runs changed no memory.max event counters and had zero OOMs.

Eight physical-query groups cover both call-stack policies and both shallow
and deep recursion. Shallow queries reached four authenticated parents and a
complete Wasm root; deep queries reached the 32-parent bound with explicit
partial status and also proved complete chains at shallower stops. Every group
observed genuine generation-two replacement callers and stale/host refusal.
Four actual console groups proved multi-frame bt and stale-stop retention.
Regression also passed eight returning/exception NI groups, four numeric
register groups, four registered CFI O0/O3 groups, four real CLI help cases
and four terminal groups with PTY Ctrl+C and PIDFD SIGINT followed by another
real Wasm step. Native stack bytes remain absent from public replies.

This qualification applies to the recorded immutable source cut on Linux
x86-64. Later concurrent changes are preserved and recorded separately. Other
platforms, a named-module package, native finish and complete external I/O
rollback are not qualified by these runs.


Qualified return-proof run on 2026-10-06
---------------------------------------

Both repositories passed 71 selected commands in the original SSH Linux
64 GiB cgroup: ten fresh full-product build/official Wasm validation commands,
four repaired physical-test compile/link commands, eight genuine physical
runtime groups and 49 regression commands. Both products retain the full
interpreter plus full LLVM SDK closure. All three product TUs were compiled
afresh in each repository. A fixture-only correction changed the second step
to continue_from_trap; those unchanged product objects were reused only
after their complete source, dependency and SDK closures were rechecked.
Four superseded first-fixture compile/link commands are excluded from 71.

The eight groups authenticated 952 first-stop return proofs and 952 fresh
proofs after another real contained instruction in the same invocation. Every
old proof was refused at that later trap. They also refused 2,856 unreadable
alias, same-pointer different-control-block and wrong-session queries, and
observed 16 genuine generation-two replacement callers. Shallow runs proved
four parents and a complete root; deep runs proved the bounded 32-parent
prefix. Returning/exception NI, public register filtering, registered CFI at
O0/O3, CLI help, PTY Ctrl+C/PIDFD SIGINT with a following Wasm step and four
actual multi-frame console groups passed.

The initial disk admission block, one shared-memory guard stop and the first
fixture's incorrect second-step entry are retained as failure history, not
successful runtime coverage. All selected owned PIDFD tasks retired and were
reaped; memory events remained at max=162 with zero OOMs. The actual 13,044
SDK inputs and immutable product source cuts matched afterward. Concurrent
working-tree changes are preserved and compared separately.

This run qualifies the preparatory return proof on Linux x86-64 SysV. It does
not qualify executable native finish, cross-owner event installation/new
parent publication, other platforms, a named-module package, full language
types or complete instance/external I/O rollback. Public replies contain
Wasm identities only; no native return address/CFA/SP/FP or stack bytes.

Protected return-proof execution transaction
---------------------------------------------

`llvm_jit_debug_resume_native_return_continuation_host_api` accepts only a
canonical sealed return owner, its original session and a synchronous trusted
host callback. It re-reads live kernel trap revision, registered CFI, bounded
stack slots and exact parent/child generation owners inside ONE real external
park domain and publication scope. Only a matching complete first-parent proof
can invoke the callback. That same scope remains held through the callback's
one-shot actual backend wake. The callback receives Wasm parent identities;
return addresses, CFA/SP/FP and copied stack bytes stay in the private unwinder.
No callback commit retains the original stop; a false wake restores actual
park flags/counts. Reentry is rejected before another domain-lock acquisition.

The genuine physical runtime fixture exercises no-commit, failed-wake rollback,
forged owners, wrong session, null callback, missing external park, stale trap
and nested-entry refusal. Its positive callback wakes another independently
proved contained native instruction and awaits a real kernel trap, then proves
the old return owner was revoked. This qualifies that protected wake join only.
Cross-owner return-event installation, a new authentic parent activation/stop,
tail/EH completion and executable native `finish` remain unavailable.

Protected-resume native qualification on 2026-10-06
---------------------------------------------------

Both repositories completed 71 selected commands in the original SSH Linux
64 GiB cgroup: eight fresh full-product compile/link commands, six corrected
fixture/official Wasm validation commands, eight physical runtime groups and
49 regression commands. Each product retains full interpreter plus full LLVM
and three freshly compiled product TUs. All 13,044 actual LLVM SDK pins and
frozen source/dependency/object/binary hashes matched afterward.

The eight groups performed 1,148 real protected contained-instruction wakes,
1,148 no-commit stop retentions and 1,148 failed-wake rollbacks. Every later
kernel trap refused its old return proof and accepted a fresh proof. They also
refused 9,284 invalid/stale/missing-domain/retired resume attempts and 3,444
nested resume entries, with 20 genuine generation-two replacement callers.
Returning/exception NI, register filtering, registered CFI at O0/O3, CLI help,
PTY Ctrl+C and PIDFD SIGINT followed by another Wasm step, and four actual
multi-frame bt console groups passed. The callback exposes Wasm parent IDs;
public native stack bytes remain zero.

All selected owned PIDFD tasks retired and were reaped; no foreign task was
adopted or signaled. The cgroup already had oom=3 and oom_kill=1 before this
run; those counters and max=166695 remained unchanged. No counters were reset.
The initial fixture's use of an implementation-private depth function, a
tmpfs reserve guard stop, and the terminal harness's rejection of an
incompatible build-record format are preserved as excluded failure history.
Only the fixture and metadata were corrected after the three product TUs;
the exact unchanged full product closure was reverified before their reuse.

This qualifies the protected proof/wake transaction on Linux x86-64 SysV.
It does not qualify cross-owner return events, a newly published parent
activation/stop, tail/EH finish completion, typed native return results,
executable ASM finish, other platforms or complete external I/O rollback.
Later concurrent working-tree changes are preserved and compared separately.


Large native caller bodies and sparse frame evidence
----------------------------------------------------

The 2026-10-06 paired repair also removes 64 KiB parent/child code scratch
buffers. Runtime-private forward MC decoding borrows only the exact current
published Wasm owner while the same execution, pause-domain, publication and
native trap guards remain held. No code pointer, return PC, CFA or saved word
escapes that scope. It never decodes a host caller or continues beyond the
logical Wasm root. Public backtrace replies still contain Wasm identities only;
the explicit 32-parent query bound is separate from dynamic Wasm frame storage.

debug_native_physical_caller_large_runtime.wat has 10,000 i64 locals in the
root and 9,999 i64 locals plus its recursive parameter in the recursive body.
It reuses the genuine physical caller runtime assertions for complete chains,
replacement generations, protected wake, failed-wake retention, stale and
forged proofs and host refusal. debug_native_registered_cfi.cc separately
checks sparse owned DATA across a 200,000-byte labelled frame, missing return
slots, unknown optional registers, duplicate and out-of-frame words and integer
overflow. That pure DATA check does not qualify live stack reading or another
architecture. These additions passed the 40-command fresh source29 qualification in both
repositories: 16 build/link commands, eight official parse/validate commands,
12 genuine physical Wasm caller groups and four actual registered-CFI groups.
Every 10,000-i64-local group proved 35 complete chains, protected wakes and
retained/rollback stops, plus four real generation-two replacement callers.
Public native stack bytes remain zero. The separate immutable source29 result
in documents/runtime/wasm-debug-full-audit-20261005.txt records the scope.
