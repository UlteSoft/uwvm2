# Debug retirement performance, v100 — 2026-10-13

Adopted in uwvm2, uwvm2-stable and uwvm2-ros. Linux tests only, inside the existing 64GiB/no-swap cgroup; no local build/run.

The hot exit path now joins workspace, checkpoint and native activation retirement in one body. A worker without a native session skips transient native-leave witness stores. All real ledgers, flags, carrier validation, tail/EH cleanup and active native reporting remain. GeneratedIR and ABI63 are unchanged; optimization flags/defaults remain unchanged.

| Repository | Input | Before ms | After ms | Time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 1998.449 | 1991.630 | 0.34% |
| uwvm2 | typed_call_ref | 3851.130 | 3551.278 | 7.79% |
| uwvm2 | gc_struct | 3880.875 | 3883.770 | -0.07% |
| uwvm2 | call_ref_fanout_256 | 1425.178 | 1386.302 | 2.73% |
| uwvm2-stable | memory64 | 1904.816 | 1921.264 | -0.86% |
| uwvm2-stable | typed_call_ref | 3914.867 | 3580.196 | 8.55% |
| uwvm2-stable | gc_struct | 3913.843 | 3983.234 | -1.77% |
| uwvm2-stable | call_ref_fanout_256 | 1447.222 | 1386.212 | 4.22% |
| uwvm2-ros | memory64 | 1912.458 | 1907.424 | 0.26% |
| uwvm2-ros | typed_call_ref | 3803.454 | 3663.032 | 3.69% |
| uwvm2-ros | gc_struct | 3915.473 | 3973.143 | -1.47% |
| uwvm2-ros | call_ref_fanout_256 | 1449.772 | 1382.617 | 4.63% |

240 fresh unprofiled kernel-clock samples, plus 24 preliminary samples. Main10 before/after pairs per input; stable/ROS5. Fresh mainordinary UWVM, Wasmtimeordinary/nativeGDB/realLLDBWasm-plugin modes are in the report. Shared host; compile/startup excluded, checksums/inputs/cache-disable/CPU21/actualtargetcapabilities checked.

36 compiled component groups and57 actual PTY groups pass. Dynamic/copiedreferences,22GCaccess traps per repository,17typed-subtype/null cases per repository and66actualdepth frames per repository are recorded in raw evidence. The existing Core3 `deterministic_profile` failure remains explicitly recorded; this is not an all-Core3-pass claim.

10 VTune driverless hardware windows: eight before/after uarch summaries, two longcall-heavy hotspots. Hardware counts, actualE-core metrics and linkedhost bridge assembly are kept separate. No newWasmtimehardwarecountercomparison is claimed.

Wasmtime parity has not been reached. Remaining costs include activation entry/admission, frame/state maintenance and ordinary dynamiccall/GC execution. Wasmtimesourcecommit `b4b23fe583173131a047141947a785b4c1c646ae` uses nativebacktraces fordebugframes andincremental state-slot stores. CurrentUWVM already has a same-module debug `call_ref` directdispatch, so it is inaccurate to assign a C++resolver to everydebugdynamiccall.

[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html) · [Intel VTune CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)

Raw evidence: `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/debug-retirement-v100-raw-evidence.tar.gz`, SHA256 `0b50c50fb84d436e8d284003b79a833c852a97cf528905bfbd852b09134dae99`. Every native member byte hash verified; old evidence andoriginalfull-flow diskguards preserved.

Mac/Windows/QEMU were not retested. Unrelated localuserexperiments are preserved by scoped overlays; the wholelocal sourcecombination was not compiled.
