# Current R3c thread creation and parked-wake samples

This independent adapter measures four already built Ordinary R3c fixtures.
It adds no compiler or counter path. Actual execution belongs to the Linux
keeper in the existing 64 GiB, swap-zero sandbox; no concurrent compilation,
VM, profiler or other benchmark is admitted.

The fixed fixture directory is
`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/current-R3c-thread-build-cold-20261003-r4`.
The r4 helper SHA is `a222edde5aad6bc9f7af2b835db86a7fb03af2ab9f08b9555bac49a73d3c1e71`;
its actual cold-after SHA is `9b02dd9197f50fc03eb9493b08b9110badb55d7c4e201c422e2446b242d020b9`.
Before and after samples the original helper authenticates the source/SDK,
actual MD, three linked fixtures, creator qualifier, official Wasm validation
and uninstrumented timed fixture. Historical cold init/PIDFD records are
source qualification; current process authority comes only from fresh host
admission and the unchanged original PIDFD protocol.

Run with the already verified toolchain loader and explicit
`RAYON_NUM_THREADS=1`, `UWVM_TEST_CPUSET=0,2,4,6,16-31`,
`PYTHONDONTWRITEBYTECODE=1`; remove `UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT`.
The keeper supplies the fresh host admission and actual static DSO inventory:

```sh
python3 run_current_thread_plain.py --admission ACTUAL_ADMISSION_JSON --perf-libraries ACTUAL_DSO_INVENTORY_JSON --out FRESH_OUTPUT --execute
```

The four exact inner commands are creation/unwind, parked/unwind,
creation/instruction, parked/instruction. Creation is `taskset -c 0
thread-timed POLICY 9 1024 FRESH_WASM_OUTPUT`; parked is `taskset -c 0,2
parked POLICY`. Generated creation Wasm bytes must equal the actual qualified
r4 fixture. No output reuses the cold qualifier file.

The adapter retains the frozen sampler, EOF/bootstrap/PIDFD cleanup,
full-process ownership/resource/environment guard and actual multi-TID
observer. Its private role derivative changes only CPU predicates: creation
all TIDs on P0; parked bootstrap/main may start on 0,2 then pins to P0,
waiter inherits that affinity before explicitly pinning to P2. Every
observed TID still requires exact UID, TGID, cgroup and allowed role affinity.
Vanished worker observations and absent printed-waiter P2 witnesses remain
qualification unknown; live mismatches reject the actual sample. No cached
identity, synthetic retirement, software counter or single-TID hardware
qualification is produced.

Whole-process `wait4` user/system CPU and maxRSS include all TIDs and the
stopped bootstrap. Parent GO-to-reap wall is separate. Creation fixture
internal fields are average per-round constructor/work/join and average
per-worker admission/body/release; two warmup samples and parsing/JIT are
excluded. Parked internal notify-to-return and notify-call exclude create,
setup and park qualification. The independent scalar checksum, exact JSON
cell sets and actual `S`/futex/notify=1 assertions must pass. Observer frequency
is a sysfs snapshot, not effective ROI GHz; temperature is observation only.
No profiler or hardware ranking follows from these samples.

The pure checker exercises synthetic JSON and source anchors only; it is not
a native test. Adapter native execution and all timing results remain pending
until the keeper returns actual receipts.
