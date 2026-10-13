# Actual r10 GC native loop audit — 2026-10-02

This audit reads the finalized native object exported by the actual r10 Linux full JIT. It does not infer code from source syntax, a hand-built IR example, or a Wasmtime result. The capture runs are cold diagnostic runs, not timing qualification.

Both products completed the self-checking 512,000,000-step workload, with 125,000 collections. Their actual object is byte-identical: 5,472 bytes, SHA-256 `efbcd8fa773a3bf489608053f6e47547244255aa186de36ab8bd0264adb1021e`. Actual llvm-nm, llvm-objdump disassembly/relocations and llvm-readobj commands all returned zero. The source IDs and executable hashes are the separately qualified r10 identities; the compile-time capture hook was enabled in those binaries and invoked only through a fresh private environment path for these captures.

The [persistent raw object, assembly and receipts](../../build/wasm3-evidence/platform-diagnostics-20261002-r10-r1/evidence/r10-cold-native-gc-loop-20261002-r1/receipts.json) are synchronized to both repositories. The mirror manifest independently authenticates each payload. This object cannot identify anonymous symbols or lambda ordinals sampled from the earlier r9 executable.

## Successful loop path

The `_func_0` loop follows these actual offsets in `.ltext` when the current sealed descriptor remains admitted, its allocation budget is available, and the table/reference bounds succeed:

| Offsets | Actual operation |
| --- | --- |
| 0x89 → 0x17c | Integer recurrence and existing-entry selection. |
| 0x17c → 0x222 | Entry/interrupt/policy/epoch checks; frontier/budget bounds; numeric cell write, live bitmap bit, accounting and committed frontier update. |
| 0x2c1 → 0x377 | Table-set admission, family/address-width, extent and exact current-range/live-token checks; complete 16-byte native carrier store. |
| 0x3d1 → 0x4af | Table-get admission/extent, complete carrier load and current-range/live-token checks. |
| 0x565 → 0x620 | Struct-get admission, exact kind/range/frontier/live checks, numeric cell address and 32-bit load. |
| 0x620 → 0x80 → 0x89 | Loop decrement and next iteration. |

There is no native call on that complete successful iteration path. Calls remain on refill, retirement, slow table/object access, collection and fatal paths. The disassembly contains no locked operation or fence on the identified successful path; ordinary loads and stores implement the target's existing acquire/release ordering. This statement covers this emitted x86-64 object and workload, not every architecture or interpreter configuration.

## Optimization candidate and safety limit

The actual object repeats entry, interrupts, pause/policy and epoch observations in allocation, table.set, table.get and struct.get. Current code also authenticates token kind/range/frontier/live bits before native payload access. This exposes a possible later bounded-span optimization, but does not justify removing checks now.

A reusable fact must be minted by the existing actual entry and remain bounded by its exact canonical descriptor, epoch, pause/admission and pinned table allocation. Every fallback, poll, native call, allocation refill, grow, exception, debug pause, retirement or collection boundary invalidates it. Asynchronous cancellation and pause requests may currently be observed at each operation; consolidating those observations requires an explicit responsiveness and no-reclamation proof. A guessed non-null context, opcode adjacency, integer slot or matching source type is insufficient.

The separate r11 collector revision changes collection-time root admission and marking. It does not change this generated guest loop or its memory access checks. Measure that revision with new source-bound binaries, semantic/ASan/OOM cases, ordinary timing, VTune hardware profiling and independent hardware counting before claiming a throughput improvement. The cold capture does not establish an industry performance ranking.
