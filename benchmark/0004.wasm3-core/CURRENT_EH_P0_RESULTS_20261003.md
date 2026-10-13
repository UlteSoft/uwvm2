# Current R3c EH: actual P0 results

Linux completed sixteen unprofiled guests and four separate pure-hardware guests using Ordinary R3c native TLS with all experiments omitted. Source ID was `sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d`; CLI SHA-256 was `e39f491639a10132a9a144f48c964ab7230b835e55e6deb3776b37b2b93edf04`. All twenty checksums/self-checks, owning-source/cache-disabled/no-body-fallback receipts and before/after closures passed. This is diagnostic evidence, not final acceptance or an industry ranking.

Persistent evidence in each repository is `build/wasm3-evidence/current-R3c-EH-P0-plain-HW-actual-20261003-r1`. The original 954447-byte archive has SHA-256 `27b60204a82a0c1eef4272b114efd87105dc1ecde8aa087d832b2ac403f6c3a5`; download origin was `/tmp/uwvm-current-R3c-EH-P0-plain-HW-complete-20261003-r1.tar.gz`. Its `independent-perf-audit.json` (9001 bytes, SHA-256 `ebbf0d49bd7a0b9c63ad10145fa795b24532e7e142ad739a8701aad7627b3a30`) separately verified all 114 payload hashes, twenty semantic logs, source closure and all four raw counting protocols. The reviewer executed no native workload locally.

## Unprofiled caught throws

The identical throws fixture executes 8M steps, asserts 500000 catches and checksum2464256. The catches are fixture assertions, not separately sampled runtime counts.

| Call-stack tracking / EH dispatch | Internal Wasm execution, two runs | Parent GO-to-reap wall, two runs | Timing quality |
| --- | --- | --- | --- |
| instruction / native-unwind | 2.431671763s, 2.422008763s | 2.441817398s, 2.432238326s | Passed sample/identity checks |
| unwind / native-unwind | 3.399756742s, 3.475327785s | 3.410882273s, 3.486093985s | Passed sample/identity checks |
| instruction / auto | 0.053638961s, 0.053683932s | 0.065781818s, 0.065818384s | Both sub100ms, unqualified |
| unwind / auto | 0.035492606s, 0.036585644s | 0.047681771s, 0.049245020s | Both sub100ms, unqualified |

Auto actually selected `r2-phase`; native-unwind selected `native`. Native caught throws are substantially more expensive in this diagnostic. In contrast, the separate 200M no-throw controls show the normal-call benefit of unwind frame tracking: instruction native internal execution was 0.603957561s with EH enabled and 0.605451891s in the exception-disabled control; unwind native was 0.144983509s and 0.145264320s. Those controls differ from the caught-throw loop and cannot be subtracted to isolate throw latency.

## Separate whole-guest hardware counts

| Call-stack / dispatch | Internal Wasm execution | Whole-guest cycles | Whole-guest instructions | Actual enabled = running |
| --- | --- | --- | --- | --- |
| unwind / native-unwind | 3.930927827s | 18036620003 | 61152874412 | 3942145134ns, 100% |
| instruction / native-unwind | 2.857401132s | 12713949110 | 44424747833 | 2867734055ns, 100% |
| unwind / auto | 0.043146040s | 256391041 | 965075438 | 57128999ns, 100% |
| instruction / auto | 0.063268136s | 344617506 | 1232787944 | 76615027ns, 100% |

All four raw non-sampling protocols passed: PMU type4, original `cpu_core/event=0x3c/` and `cpu_core/event=0xc0/`, successful FD5 leader/FD7 member with group_fd5, real enable ACK before guest GO, one actual P0 guest TID, and successful original-PIDFD SIGINT after actual guest wait4/PIDFD retirement. Enabled and running nanoseconds came from raw perf output; no reference-cycle event, clock reconstruction, scaling or software fallback was added. Auto's counts are valid while its sub100ms timing samples remain unqualified.

Internal execution excludes startup/JIT but includes the complete Wasm workload. Hardware counts cover startup/JIT/execution/output. Parent wall and wait4 user/sys/RSS are separate; wait4 includes stopped bootstrap. Different plain/HW samples cannot be combined to derive ROI GHz or CPI. Sysfs frequency is only a snapshot distribution; temperature was observation only. Host SMT noise and loaded DSO maps were not independently qualified. Actual boot was `83f582ec-ff7f-41ce-9631-2c0fbb865aeb`.

## Next attribution

Actual source first builds throw payload fields, captures an immutable trace, then performs the native throw. Unwind trace capture walks bounded real CFI frames, resolves published PC identities and merges mixed T0/native frames. Name resolution is lazy at display; neither 4096-frame capture nor per-throw name copying explains this source. These counts alone cannot divide cost among backtrace, PC lookup, payload/trace allocation and native search/cleanup. The three hardware VTune controls remain a separate evidence family; use actual target PID/TID/P0 samples, stack-loss and MUX limits, and keep unknown mappings unknown.

No trace, tag, root or CFI contract was weakened. Any proposed EH optimization still requires uncaught colored diagnostics, catch_ref/throw_ref/rethrow trace retention, mixed-call stacks and concurrent code/root lifetime controls. Actual thread create/join and parked wake require their separate multi-TID plain ROIs.
