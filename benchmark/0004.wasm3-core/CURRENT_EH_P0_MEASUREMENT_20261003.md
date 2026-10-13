# Current Ordinary EH P0 adapter

Source-only v1 binds the actual prepared `current-R3c-EH-P0-order-20261003-r1/plan.json` (7044 bytes, SHA-256 `700f5c02679e8c922abeb05d70ab9a5dd6b1492ac478150bf7041416d5834992`). Its twelve current complete-CLI cold cells passed functionality on E-cores; they are not P-core timing. Ordinary source ID is `sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d`; CLI SHA-256 is `e39f491639a10132a9a144f48c964ab7230b835e55e6deb3776b37b2b93edf04`. ROS R3f has its own source/build/cold and is not silently admitted by this Ordinary adapter.

The new runner imports the immutable EH order/parent, `run_general_gc_R5_nativeTLS.py` v4, `run_current_general_gc.py` (91ee), host counter (673d), hardware protocol (266a), and base (0ab4). It reuses their stopped-child/capture-clear witness, terminal-state guard, plain sampler, actual PMU attribute/group parser and owned SIGINT completion. It does not define a new guardian, change the old plans, add events or widen single-TID counting. A reference sentinel only disables the unused Wasmtime terminal-recovery branch; every admitted guest is the fixed Ordinary product.

The closed parent must still match all three fresh-TU native-TLS compile prefixes, source fingerprints, MD inputs, objects, SDK/LLVM/static archives, loader files and the exact independently finalized generated Python-cache delta. The original full input inventory is not claimed byte-equal. Actual cold/raw log, fixed Wasm, current complete product and loader closures are rechecked before/after. Fresh actual host admission independently checks the current boot/init birth, exact cgroup, 64-GiB memory/swap0, CPU set, own process roster and P0 guest/TID. Historical cold PID 10154 is not permission to run on any later boot.

## Execution contract for the sole Linux keeper

Stage this source freeze with its pinned dependencies, then use the keeper's already reviewed stopped host-controller launch wrapper and a fresh host admission/47-file perf DSO inventory. Run sequentially with no builds, VM tests or profiler in the same measurement window. The controller belongs to that actual 64-GiB cgroup; the original guard pins its housekeeping to an E-core and each guest to P0. Temperature is raw observation, never a start/peak/pair rejection.

Controller environment is fixed to the command plan's four assignments: the actual `LD_LIBRARY_PATH`, `RAYON_NUM_THREADS=1`, `UWVM_TEST_CPUSET=0,2,4,6,16-31`, `PYTHONDONTWRITEBYTECODE=1`. Clear `UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT`, `LD_PRELOAD`, `LD_AUDIT`, `LD_PROFILE` and profiler injection variables. The inherited capture key is additionally stripped by the immutable Popen adapter; its real stopped `/proc/<pid>/environ` hash and absence proof are saved before guest GO. No ASM capture is enabled during timing.

The following are arguments to the existing keeper launch wrapper, not unbounded host shell launches. `SOURCE_PACKET` contains this runner and its immutable peers; `ACTUAL_ADMISSION`/`ACTUAL_PERF_LIBS` are that window's actual pinned receipts. Use fresh `OUT_PLAIN` and `OUT_HW` directories.

```text
python3 SOURCE_PACKET/benchmark/0004.wasm3-core/run_current_EH_P0_measurement.py
  --plan /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/current-R3c-EH-P0-order-20261003-r1/plan.json
  --admission ACTUAL_ADMISSION --perf-libraries ACTUAL_PERF_LIBS
  --out OUT_PLAIN --measurement plain --execute

python3 SOURCE_PACKET/benchmark/0004.wasm3-core/run_current_EH_P0_measurement.py
  --plan /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/current-R3c-EH-P0-order-20261003-r1/plan.json
  --admission ACTUAL_ADMISSION --perf-libraries ACTUAL_PERF_LIBS
  --out OUT_HW --measurement pure_hw --execute
```

Plain executes the twelve fixed fixture × call-stack × exception-dispatch cells plus four throws cells in reverse order: sixteen guests total. Hardware executes only the four throws cells. The grouped actual `cpu_core/event=0x3c/` and `cpu_core/event=0xc0/` events are non-sampling, no-inherit, no-scale, attached to this guest's single TID. Actual FD/group relation, final PMU type/config, enabled/running raw values and enabled ACK-before-GO are mandatory. Only successful original PIDFD SIGINT after the guest's actual retirement permits the documented perf SIGINT exit. No reference-cycle support or effective GHz is invented; no software fallback is allowed.

## Semantic and timing boundaries

Each guest must exit zero from its exact self-checking fixture. `plain_normal` and `eh_normal` run 200M steps with checksum 1231817216 and zero catches; `eh_throws` runs 8M steps with checksum 2464256 and 500000 catches. These are source-verified fixture properties, not fabricated runtime counter measurements. The current compiler receipt must independently report owning source, disabled object cache, no body fallback and exactly the cold-bound actual plan: `native` for explicit native-unwind and exception-disabled plain controls; `r2-phase` for EH auto. Known `module=<integer> actual-epoch=<integer>` suffixes are recorded, never treated as publication permissions. Other optimize/finalize compiler messages do not masquerade as duplicate materialization receipts.

The actual `Total WASM execution time` and `Total process time` must appear once each, be positive integral nanoseconds and be ordered consistently. EH does not require a GeneralGC allocation/reclamation record. Auto dispatch and native-unwind dispatch remain separate from instruction/unwind call-stack tracking.

| Measurement | Exact scope |
| --- | --- |
| Internal Wasm timer | Excludes startup/JIT; includes loop, checksum and EH bookkeeping. Not isolated throw/catch latency. |
| Parent GO-to-reap wall | Actual complete process after stopped GO, includes startup/JIT/output. |
| wait4 user/sys/RSS | Whole child lifetime, including stopped Python bootstrap; separate from internal timer. |
| Pure hardware events | Whole guest single TID including startup/JIT; not the Wasm timer's ROI. |
| Frequency observations | Raw sysfs snapshots; not effective ROI GHz or reference-cycle-derived frequency. |
| VTune | Separate profiled runs, PMU multiplexing/sampling/stack/environment limits retained. |

Short auto plain samples may finish before actual loaded argv or an in-window frequency observation. The original sampler retains null/unknown evidence and marks the sample unqualified; no padding, synthetic identity or frequency is introduced. Hardware ownership/identity and ACK/counting remain strict. Host SMT noise is unknown without an independent real observer. An incomplete before/after series is never complete qualification. `formal_acceptance` remains false even if all guests and counters pass.

## Separate hardware VTune family

The order's `vtune-commands.json` has two native-throw Hotspots controls (instruction and unwind call-stack) and one unwind/native uarch summary. They remain templates for the keeper's approved actual-process-tree VTune supervisor, with real installed help/knobs and exact product/fixture/environment pins. No unsupported `-cpu-mask=0` option is inserted; the guest is pinned P0 by the original process protocol. Actual result reports must filter target PID/TID and `cpuid=cpu_0`, verify P0 partition totals and retain native DSO/address provenance. Intel's injected loader prefixes must be recorded; a strict environment mismatch stays unqualified, not retroactively accepted.

Use hardware `hotspots` with actual supported sampling/stack/characterization/perf-worker controls and `uarch-exploration` with actual supported summary mode. First collect the real installed help/version, then use the unchanged reviewed successful hardware launch argv, replacing only this actual source-bound target/result directory. No SW collection. `_Unwind_Backtrace`, real RaiseException/search, compact-trace/payload allocation and native collector/root helpers are candidates for actual symbol attribution, not presumed causes. Skipped-stack/MUX warnings and unknown JIT mapping stay explicit. Do not subtract the 200M no-throw workload from the 8M throws workload to invent a per-throw cost.

## Source checks

`PYTHONDONTWRITEBYTECODE=1 python3 check_current_EH_P0_measurement.py` performs pure Python shape/parser negatives without spawning a child. An optional directory argument reads the four actual prepared order JSON files and checks their original lengths/hashes. These checks do not establish native execution, CFI correctness, newboot profiling or performance improvement.
