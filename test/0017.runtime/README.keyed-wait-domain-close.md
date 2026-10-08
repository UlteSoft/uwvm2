# Durable close of resumed native waits

A cooperative pause-domain close must remain visible to a linked infinite
native wait after resume. A notification alone is insufficient: if close clears
the pause flag, a waiter can wake, see a false predicate and sleep indefinitely.

The domain publishes an atomic closed state before waking registered waiters.
Only the external wait policy uses the pause-or-close hint. Ordinary generated
pause polling still reads its original pause flag. The hint grants no paused
cohort, checkpoint or shutdown authority.

The waiter reads the hint without taking the domain mutex under its wait-shard
mutex. Actual suspension releases the shard before polling the domain. Existing
notification, stop-token, registry-close and timeout arbitration remains intact.

keyed_wait_domain_close.cc executes two real workers, two pauses and resumes,
then closes after workers can re-enter the infinite wait. It tests ordinary and
pre-registered wait queues, 20 repetitions each. Every passing cancellation is
followed by physical joins, drain and an empty-queue check. Comparisons must not
repeat. A bounded notification rescue cleans up a failed test but cannot make it
pass. Defining UWVM2TEST_OLD_PAUSE_WAKE_PREDICATE reproduces the old predicate and
must fail with exit 1.

Native O3, ASan/UBSan, the existing wait arbitration tests and actual C++ module
consumers are separate checks. The Wasm blocked-cohort test additionally checks
live GC locals, dynamic operand prefixes, four memory/compare-width combinations,
instruction/unwind strategies, observe/resumable recording and both real notify
and managed shutdown. Its shutdown branch deliberately delays after the second
resume to cover the lost-wake window.

Run Linux builds and tests only through the required 64 GiB, swap-disabled
cgroup entry and the bounded native resource guard. The qualified R25 commands,
input hashes, binaries and guard receipts are recorded in the native evidence
directory. Never interpret a sleep timeout, rescue notification or an assertion
trap as successful Wasm cancellation.

This fix does not implement parallel automatic GC, forcibly cancel arbitrary
host I/O or supply native AArch64/PPC64 performance evidence.
