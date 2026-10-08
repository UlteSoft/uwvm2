# General GC fixture and measurement contract

This is source preparation for the keeper's next small cold round. No new
fixture has an official validator, Wasmtime, product, native, or performance
pass yet. Existing r11 two-header changes, the approved ABBA/counter wrappers,
the immutable compact native component and their recorded evidence stay frozen.

`generate_general_gc.py` emits an actual Core 3 binary, readable WAT and a
scalar-oracle manifest without invoking a VM or external compiler. The binary
uses the official [GC instruction encodings](https://webassembly.github.io/spec/core/binary/instructions.html)
and [recursive, mutable field and reference type encodings](https://webassembly.github.io/spec/core/binary/types.html).
The first round uses 65,536 iterations; accepted generator bounds are
1,024 through 2,000,000. This keeps both oracle construction and first failures
small. Binary/WAT agreement remains an independent remote check.

## Workload families

| Family | Objects per allocation group | Payload and edges | Main-loop observations |
| --- | ---: | --- | --- |
| `mutable-struct` | 1 | one mutable i32 field | `struct.set`, actual `struct.get`, data-dependent checksum |
| `reference-cycle` | 2 | A and B have mutable i32 and nullable node fields; A→B→A | change A→A, read through that edge, restore A→B and read B, assert B→A identity |
| `numeric-array` | 1 | eight mutable i32 elements | write two distinct rotating lanes, read both and actual length |
| `reference-array` | 3 | eight mutable node references plus A→B→A | store B and A to distinct rotating lanes, read both payloads and actual length |

Every family has two independent phases. `allocate` repeatedly replaces one
of 1,024 exported ring roots and first reads the previous object before
replacement after the first full ring. This forces old objects to survive the
intervening allocation pressure. `mutate` allocates exactly 1,024 groups during
setup and performs zero allocations in its main loop. It isolates a bounded
retained set but its total run time still includes setup and final readback.
The manifest records main versus setup allocations explicitly. Neither phase
is a stopwatch for a single allocator, collector, mutation or lookup operation.

Reference-array has a second exported 1,024-slot table rooting A, enabling
unambiguous access to A after arbitrary array-slot updates. It therefore has
2,048 table root slots, including duplicate reachable objects. Its final
reachable set is still 3,072 objects; the other families have 1,024 table root
slots and 1,024 or 2,048 reachable objects. Do not compare its root traversal
cost as if all four families had identical root snapshots. No fixture tests a
foreign store, host externref or concurrent mutator; those remain separate tests.

The LCG seed is 123456789 and the update is `state*1664525+1013904223`
modulo 2^32. The main checksum depends on actual updated payloads and prior
root readback. The final traversal reads every ring entry and every array lane,
checks cycle identity/array length, and publishes separate `checksum` and
`root_checksum` globals. Exported `run` returns their XOR. `_start` traps if
either global, the last LCG value, or the returned value differs from the
independent expected result. All tables escape through exports. This rejects
missing/wrong work semantically; a correct compiler may still optimize
semantically redundant computations. Product actual allocation counters and
finalized code inspection are required to identify representation/elision.

The cycle fixtures deliberately drop intact cycles on table replacement.
Exit zero alone does not prove collection. For an allocation row, require
actual collections and reclaimed objects greater than zero before calling it
collector coverage; retain measured peak RSS and counts at two workload sizes
before claiming bounded cycle reclamation. No exact collection/reclaimed count
is embedded: the final allocation interval, live locals and automatic trigger
policy vary. A final table-root count is not the runtime's complete root count.

## Small cold handoff, exclusively on Linux

Generation is pure Python and can be done inside the keeper's admitted scope:

```sh
python3 benchmark/0004.wasm3-core/generate_general_gc.py --family mutable-struct --phase allocate --iterations 65536 --out-prefix <new-fixture-dir>/mutable-struct-allocate-65536
```

Repeat for the four named families and two phases, each with a new prefix.
Do not use a destination that already exists. Preserve the generator SHA,
all three output SHA values and the exact command. The later directory must
contain the same `.wasm` bytes supplied to every product and Wasmtime. Nothing
in this document supplies a fake product/source ID or runnable placeholder.

For each eight-fixture set the keeper must preserve actual tool ELF/version
and `--help` output, run official `wasm-tools validate` on the binary, parse its
WAT using official tools and byte-compare against the emitted binary, and
retain official printed WAT. Text re-encoding may introduce a name custom
section; use no generated names and record any exact section difference rather
than silently replacing the original bytes. Execute those original bytes with
the pinned installed Wasmtime using its confirmed GC/copying feature options,
then the products. Every `_start` must exit zero. Also invoke exported `run`
with Wasmtime in a fresh instance, parse its actual i32 return modulo 2^32 and
compare the manifest. Inspect current installed help for invoke syntax; no
guessing, software profiler or unrelated WASI implementation is needed.

Start product cold coverage with LLVM full and interpreter full in both
repositories, then ordinary lazy/tiered modes when their actual build receipt
supports them. Keep required Wasm feature admission identical. In a separate
cold negative control, replace only the wrapper's expected return constant with
a different legal i32 constant; official validation should still pass, and all
VMs must trap. Preserve both hashes and actual stdout/stderr. This is an
independent executed oracle-strength control, not a malformed bytecode test.

All executable VM/compiler/test work stays in the keeper's verified 64 GiB,
swap-zero scope. P0 is for timing, admitted E cores for compilation/cold checks;
never overlap a timing row with our Windows/compiler/other profiler work. Use
the existing actual ownership, memory, deadline and output bounds. This
document does not weaken or substitute a guard and does not make the frozen
GC-specific counter wrapper accept arbitrary fixtures.

## Build/profile separation

Bind two genuine build profiles per product to actual source fingerprints,
RT/CLI compiler command sidecars, compiler/rsp/archive closure and ELF/runtime
hashes. `default` has all six experimental switches explicitly zero or proven
absent in both translation units. `experiment6` has exactly these six at one:
`COMPACT_NUMERIC`, `MANAGED_NUMERIC_PAGE`, `SEALED_COMPACT_CURSOR`,
`SEALED_LOCAL_TABLE`, `PENDING_NUMERIC_FUSED_CATCH`, `PACKED_NUMERIC_ARRAYS`,
each with the `UWVM_EXPERIMENTAL_` prefix. Reject conflicting separate/joined
defines, undefines, forwarded overrides or unbound response files. ABI,
compiler, optimization, tracing policy, runtime mode, features and combine/
delay flags otherwise match. Same source snapshot is preferred; any other
source difference makes it a general build comparison rather than a macro
effect. Do not reuse experiment6 RT under a default CLI.

None of the four type layouts is eligible for immutable one-field compact
numeric admission. The mutable numeric struct can use the existing legacy
numeric slab. Numeric arrays can use the experimental packed numeric tail;
they still require their real mutable array safety checks. Reference-bearing
objects require real graph semantics under both profiles.

After all cold checks, a first development round can run default/experiment6
in ABBA order for one allocation fixture and its mutate control, then repeat
for the remaining families. Increase iterations only after bounded completion
and actual collection coverage. Report whole guest, process wall, user/system
CPU, RSS, actual allocation/collection/reclamation/root counters and frequency
P05/median/P95 with sample counts and host sibling activity. Temperature is
observation only. Short rows with inadequate frequency observations remain
unqualified. Collect pure hardware cycles/instructions and VTune samples in
separate later sessions; preserve their different intervals and MUX/stack
limits. Never divide whole-VM time by allocations and call it pure GC latency.

Existing roughly 117 ns versus roughly 9 ns observations concern different
historical whole workloads/profiles. They do not establish default GC,
mutable/reference object performance, isolated collector cost, or an industry
ranking. The actual general-family result table is currently empty.

## Local source preparation receipt

`check_general_gc_generator.py` performs 24 small scalar-oracle/binary-shape
controls and 11 negative encoder/parameter/truncation controls. It runs no VM,
official validator, native executable, guard or profiler. Both repositories
must have identical generator/checker bytes. Its receipt explicitly keeps all
runtime/performance acceptance fields false. Frozen existing two-header,
ABBA/counter and compact-component source checksums remain unchanged.
