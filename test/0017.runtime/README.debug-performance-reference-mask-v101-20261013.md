# Reference validation performance, v101 — 2026-10-13

Adopted in uwvm2, uwvm2-stable and uwvm2-ros. Build, runtime and sampling tests ran only in the existing SSH Linux 64GiB/no-swap cgroup. No local build or runtime test ran.

Sealed metadata now holds each reference slot's index and allowed carrier kinds. Every current kind, initialization flag, extent and required zero carrier is still checked. Parser function indices and unknown tags remain rejected. The mask grants no subtype, store, root, activation or execution authority. Numeric shapes and all metadata-retention quotas remain unchanged.

Generated LLVM IR, ABI63, nativeABI2 and optimization defaults are unchanged. C++ module imports were synchronized, but module-mode compilation was not tested.

| Repository | Input | Before ms | After ms | Time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 1898.809 | 1899.076 | -0.01% |
| uwvm2 | typed_call_ref | 3426.017 | 3305.043 | 3.53% |
| uwvm2 | gc_struct | 4283.343 | 4284.608 | -0.03% |
| uwvm2 | call_ref_fanout_256 | 1428.801 | 1406.444 | 1.56% |
| uwvm2-stable | memory64 | 1949.810 | 1937.541 | 0.63% |
| uwvm2-stable | typed_call_ref | 3431.612 | 3341.144 | 2.64% |
| uwvm2-stable | gc_struct | 4387.055 | 4355.824 | 0.71% |
| uwvm2-stable | call_ref_fanout_256 | 1481.604 | 1463.961 | 1.19% |
| uwvm2-ros | memory64 | 1939.670 | 1937.669 | 0.10% |
| uwvm2-ros | typed_call_ref | 3427.507 | 3415.277 | 0.36% |
| uwvm2-ros | gc_struct | 4419.971 | 4386.338 | 0.76% |
| uwvm2-ros | call_ref_fanout_256 | 1476.041 | 1456.347 | 1.33% |

240 fresh unprofiled kernel-clock samples plus24 preliminary samples. Main10 before/after pairs per input; stable/ROS5. Fresh ordinaryUWVM and Wasmtime ordinary/nativeGDB/realLLDBWasm-plugin modes are separate. Input/checksum/cache-disable/CPU21/actual target capabilities were checked; compile/startup excluded. The host is shared.

45 compiled component groups and57 realPTY groups passed. The new450carrier cases ran per repository: nullable/abstract/defined heaps, parser/unknown/large tags, old-predicate equivalence, real awaiting-parent admission and validation before visitors. GC traps, typed subtypes and66actualdepth frames are in raw evidence. The existing Core3 deterministic_profile failure remains recorded; this is not an all-Core3-pass claim.

10 VTune driverless hardware windows: eight before/after uarch summaries and two long call-heavy hotspots. Linked host assembly and hardware counts are kept separate; no new Wasmtime hardware-counter comparison is claimed.

Wasmtime parity has not been reached. Remaining work includes actual frame entry/state maintenance and ordinary dynamic call/GC paths. A successful fixed-rule optimization is not removal of those costs.

[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)

Raw evidence: `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/debug-reference-mask-v101-raw-evidence.tar.gz`, SHA256 `049319183ffdd9b1f5e0645bb3af1cb9365afc152d939d35eb27c4baff93746c`. Every native member byte hash was verified; old evidence and original full-flow guards were preserved.

Mac/Windows/QEMU were not retested. All unrelated local user experiments are preserved; the full local combination was not built.
