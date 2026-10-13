# Compiler-selected ordinary return, v104 — 2026-10-13

Adopted in uwvm2, uwvm2-stable and uwvm2-ros after source/binary qualification. Build, runtime/debugger tests and VTune ran only in the existing SSH Linux64GiB/no-swap cgroup. No local build or runtime test ran.

Linuxx86_64 Observe ordinary returns select a two-argument native bridge. Its complete existing retirement is compiled with constant kind0, permitting tail/EH branches and one guest-call argument setup to be removed. Authentic frame, epoch, parent, publication, workspace and active native-session checks remain. Typed/host tails, EH, other modes and other platforms keep existing selection.

Native step recognition joins the real captured frame and exact specialised bridge identity; its workspace argument is not treated as an exit kind. The actual bridge still authenticates the LIFO buffer. Observe cache policy64 rejects old63 generated objects. Resumable17/native2/schema6/disk-format5 and optimization defaults unchanged.

| Repository | Input | Before ms | After ms | Paired time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 1898.888 | 1900.651 | 0.07% |
| uwvm2 | typed_call_ref | 3256.368 | 3230.747 | 1.11% |
| uwvm2 | gc_struct | 3884.755 | 3881.799 | 0.07% |
| uwvm2 | call_ref_fanout_256 | 1359.940 | 1342.468 | 1.35% |
| uwvm2-stable | memory64 | 1895.552 | 1895.522 | 0.01% |
| uwvm2-stable | typed_call_ref | 3248.109 | 3245.089 | 0.07% |
| uwvm2-stable | gc_struct | 3903.681 | 3918.400 | -0.24% |
| uwvm2-stable | call_ref_fanout_256 | 1370.223 | 1352.882 | 1.05% |
| uwvm2-ros | memory64 | 1895.415 | 1896.340 | -0.05% |
| uwvm2-ros | typed_call_ref | 3251.810 | 3241.622 | 0.31% |
| uwvm2-ros | gc_struct | 3914.026 | 3905.451 | 0.23% |
| uwvm2-ros | call_ref_fanout_256 | 1370.771 | 1353.555 | 1.14% |

240 fresh unprofiled kernel-clock samples plus24 preliminary samples. Main10 alternating before/after pairs per input; stable/ROS5. Fresh ordinaryUWVM and Wasmtime ordinary/nativeGDB/realLLDBWasm-plugin modes separate. Input/checksum/cache-disable/CPU21/actual target capabilities checked, compile/startup excluded. The host is shared.

48 compiled component groups and57 realPTY groups passed. The new realMCJIT return-emitter test executes ordinary returns, typed/host tails, EH and non-observing fallback. Reference/packet transitions, metadata bounds, cold failures, full cache identity and authentic native stepping/cancellation were rerun. Further GC traps, typed subtypes and66actual-depth frames per repository are in raw evidence. The existing Core3 deterministic_profile failure remains recorded.

10 serial VTune driverless hardware windows: eight before/after uarch summaries and two long call-heavy hotspots. Host assembly static counts and actual hardware counts kept separate. No fresh Wasmtime hardware-count comparison claimed.

v102 helper inlining and v103 frame-local flag trials were rejected for measured timing/cycle regressions and restored exactly. Fewer static/helper instructions alone do not justify adoption.

Wasmtime parity has not been reached. Actual frame admission/state maintenance and ordinary execution/translation costs remain; ratios alone cannot assign the whole difference to debugger bookkeeping.

Raw evidence: `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/debug-observer-return-v104-r2-raw-evidence.tar.gz`, SHA256 `c065c7388d6b1f21b3db81c6a0d257b5dcd6cae91ddb24c3e19da6acbecb4a04`. Native member byte hashes verified; previous evidence and original guards preserved.

Mac/Windows/QEMU and C++ module builds were not retested. All unrelated local user experiments preserved with scoped emitter overlays; the full local combination was not built.
