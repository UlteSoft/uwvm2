# Checkpoint identity revision 2 DATA integrity

This source candidate binds every function generation/body and the complete saved
replay log to the format4 identity before encode publication or decode output.
It remains DATA only: successful comparison cannot issue a checkpoint, restore,
stop, host capability or native continuation.

Function domain UWCPFG02 hashes module ordinal, original module SHA256, number of
instantiated instances and number of unique instance/declaration function keys.
Each key hashes the dense per-module instance ordinal, u16 host/Wasm flags, u64
local declaration index/generation/type index, u64 replacement-body byte count
and those bytes. Host keys additionally hash u16 adapter flags and u64 adapter
ID/version. Generation1 Wasm bodies use original module bytes and an empty body;
replacement generations record nonempty exact local-declaration/body bytes.
Equal same-instance/declaration aliases deduplicate; contradictory records fail.
Different instances may have independent generations. File-local store IDs and
native addresses never enter the code closure. The actual runtime issuer must
resolve the dense logical instances and code against genuine publications.

Event domain UW CPEV02 (without the space) hashes the exact recording ID, u64
replay cursor, u64 resident event count and every full format5 canonical event
record (all flags, eight words, capability IDs, typed values and adapter bytes).
It includes resident events after the cursor so later replay data cannot be
changed by merely recomputing the surrounding checksums. No event data is proof
that an adapter actually executed it. The genuine producer/scheduler still needs
its own replay witnesses and adapter compatibility checks.

Identity revision2 uses UWCPID02. Outer envelope4, state5, cache5 and fixed field
widths are unchanged. Revision1 is deliberately rejected rather than guessing
its former label-based semantic hashes. Both products must update together.

New C++ regressions repair every surrounding checksum and change bodies,
generations, type indices, event arguments/results/payload/error/effect, future
events, cursor and recording ID. Decode must reject without changing old output;
encode must reject before writing. The test also covers canonical Python hashes,
multiple instances, contradictory aliases, aggregate quotas, invalid reserved
pointer tokens and DATA-only authority. Existing envelope C++/independent Python
fixtures are updated to derive rather than label their state semantic hashes.

Compile/run is pending on Linux and qualified big-endian QEMU in the existing
64 GiB cgroup. No local/native test was performed for this source candidate.

This cold identity closes the state5/envelope4 source dependency. Module flags1
retain the separately resolved syntax policy; canonical state-body bytes bind
those fields. The function closure covers original/effective function code,
not authority to select a compiler/feature policy. Actual compilation context
includes the real per-module syntax tuple and profile17/native ABI2; all original
source, current body/generation and private new-world ownership still require
genuine runtime proof. No cache acceptance, native epoch or whole-world restore
is granted by this detached DATA helper.
