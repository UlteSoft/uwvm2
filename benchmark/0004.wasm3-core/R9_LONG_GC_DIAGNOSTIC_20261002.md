# r9 long GC development evidence

Ten P0 executions completed with exit zero and matching semantic receipts in
one 128M/512M pair. This is development evidence, not an engine ranking or a
complete release acceptance. Temperature is an observation only under the
latest user criteria. The existing pair median-frequency rule passed; host
P0/SMT quiet was not established.

The recoverable repository packet is
[`build/wasm3-evidence/performance-20261002-r9-long`](../../build/wasm3-evidence/performance-20261002-r9-long/root-mirror-manifest.json).
Its manifest SHA-256 is
`3992409ffe0cdc7aa8c0185dc15c0b5b4449d379091cb32a7a3c02402809f719`.
All 20 payload files, totaling 10,843,258 bytes, were independently checked
against manifest sizes and real SHA-256 in both repositories. The
[raw run](../../build/wasm3-evidence/performance-20261002-r9-long/r9-long-development-diagnostic-20261002-r4/raw.jsonl)
has SHA-256 `1689f18cd5f028b7928caff97379e9990ea3cd90d726d490c758512e5ec07357`.
Adjacent summary, runner, plan, all logs and each before/after source receipt
are preserved. The ordinary before and after receipt files independently hash
to `0d2d532d05375a4810d375f08762b0609d8c67e22078b8fad2a7975d9cc01e76`;
ROS independently hashes to
`b454f7c0ddf6923678089d7f9ea99f1fdd09b633bd2a025f6908aea40c9579c0`.
Those actual receipt source IDs match the summary. Host observation is the
sibling `r9-long-diagnostic-host-observer-20261002-r4` directory. The original
transfer location `/tmp/uwvm-r9-evidence-20261001/evidence/` is an origin only,
not the primary recoverable evidence.

| Profile | Wall high-minus-low ns/step | Guest high-minus-low ns/step | Maximum RSS MiB |
| --- | ---: | ---: | ---: |
| uwvm2 instruction | 9.743085 | 9.743282 | 49.33 |
| uwvm2 unwind | 9.737045 | 9.738543 | 50.00 |
| uwvm2-ros instruction | 9.640997 | 9.640379 | 48.78 |
| uwvm2-ros unwind | 9.750287 | 9.747543 | 49.23 |
| Wasmtime 49 copying | 10.953671 | unavailable | 27.40 |

The slopes divide the observed 512M-minus-128M duration by 384M iterations.
Product guest durations come from their `Total WASM execution time` logs;
Wasmtime supplies process time only. Product wall-minus-guest was approximately
21–22ms at both sizes, so a fixed startup/materialization cost does not account
for the observed product guest slope. This is one pair with no confidence
interval; the small differences between product modes do not establish a
performance difference.

All eight product executions recorded exact allocation and committed-slot
counts. Each 128M run performed 31,250 collections and reclaimed 127,998,975;
each 512M run performed 125,000 collections and reclaimed 511,998,975. Disabled,
reason, peer/policy skips and heap/population rejections were zero. The pinned
`_start` traps on a wrong checksum; exit zero is that proof, not a printed
checksum value. Each log reported native owning-source, object cache disabled
and no body fallback.

Sealed retirement was exactly one in every product run. At 128M the real
refill/window/poll-attempt count was 156,250; at 512M it was 625,000. Deferred
allocation counts were 127,843,750 and 511,375,000, respectively; remaining was
zero. Those data contradict a proposed explanation involving table-get
retirement on every ring iteration. The pinned loop allocates a new struct,
writes it to a slot and immediately reads that same slot. The 1024 older table
roots still determine GC liveness. Counters alone do not identify the cost of
native refill, collection or generated checks; VTune and loaded machine code
are the next attribution evidence.

The product median sampled P0 frequencies were approximately 4.900GHz;
Wasmtime medians were 5.080/5.100GHz. Their cross-pair median spread was about
4.08%, below the existing 10% rule. Individual samples included brief lower
frequencies; a median is not proof of constant active frequency. Temperature
peaks, including a 106°C observation, remain raw observations and are not a
rejection reason.

Aligning the host observer's actual timestamps with these ten guest intervals,
and excluding the real guest cgroup authenticated through its birth/affinity,
found 17 other active task records whose last CPU was P0 or its SMT sibling:
16 WebKit `HeapHelper` records and one Wi-Fi IRQ record, all on sibling CPU1.
There were also 748 active records with eligible broad affinity and incomplete
residency information. The observer's actual consecutive timestamp median
was approximately 154–159ms during the runs, despite its nominal 50ms setting;
residency between samples remains unknown. No foreign process was signaled.

Input/output closure passed before and after all ten executions. The runner
SHA-256 was `0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89`,
and plan SHA-256 was `b1de470181c3eceefb47fdca2a76f62e2361f19499b2302b79ed604341412f8c`.
The ordinary source was `sha256:5f433a669066a9a35401648b7c76d87745692ca0e507455e5e07951e648cb1e5`,
ELF `17f156ce090788a3a0306ab9d5808c9db501f14552bbe631a4507d2ebe264b74`.
The ROS source was `sha256:c680525917601070275ff61b7cf5fd90b5c5e07d4b196451c6dba4a9679d987a`,
ELF `a5336b707f785116a56c4473d3dd289853d0893d051f1bc1c9213e51b1eff8ad`.
Both actual builds enabled the same six reviewed GC/EH experiments; these
results do not establish the performance of a build with those macros off.

The earlier failed attempts are preserved separately: missing verbose timing
in the first completed guest, zero-guest disk admission rejection, and the
sixth r3 guest's owned SIGKILL after actual `memory.swap.max` changed externally
from zero to `max`. They are not substituted into the complete r4 pair.

## Additional fixed-source serial controls

A later serial round completed 20 GC guests in two AB/BA pairs, 27 EH guests
and ten memory64 guests. All 57 exited zero, passed semantic receipts and
source/ELF/fixture closure, and had no hard failures. These use the same r9
ELFs and plan above; they do not measure the later r10 source. The recoverable
[small-control mirror](../../build/wasm3-evidence/performance-20261002-r9-small/curated-mirror-manifest.json)
contains 85 payload files, 26,450,136 bytes per repository; both mirrors and
origin files were independently checked. Manifest SHA-256 is
`2eae5bb6a8e09779d2b5b6682fcc0419894428c22a2346471c07226277fbe1d5`.
The `/tmp/uwvm-r9-evidence-20261001/evidence` transfer is an origin only.

| GC profile | AB wall / guest ns per step | BA wall / guest ns per step |
| --- | ---: | ---: |
| uwvm2 instruction | 9.121685 / 9.123363 | 9.227200 / 9.227251 |
| uwvm2 unwind | 9.241048 / 9.240981 | 9.310970 / 9.306446 |
| uwvm2-ros instruction | 9.244555 / 9.246430 | 9.207654 / 9.208498 |
| uwvm2-ros unwind | 9.263478 / 9.263523 | 9.320455 / 9.321385 |
| Wasmtime 49 copying | 10.565857 / unavailable | 10.551868 / unavailable |

These again divide actual 512M-minus-128M times by 384M steps; no profilers
were active. All product GC counts exactly match the original pair. The
product median observed frequencies were about 5.1–5.3GHz, higher than the
earlier 4.9GHz pair. Faster elapsed time at the same ELF is therefore not
evidence of a code improvement. Two pairs remain insufficient for an engine
ranking or a confidence interval.

Product frequency telemetry aligned to the log's `Begin running` and
`Total WASM execution time` UTC envelope gives the following 512M
P05/median/P95 observations. Quantiles use linear interpolation at `(n-1)*q`.
The envelope is approximate wall-clock alignment, not an exact PMU ROI;
`scaling_cur_freq` is a sampled observation rather than active-cycle frequency.

| Product / trace | AB GHz P05 / median / P95 (n) | BA GHz P05 / median / P95 (n) |
| --- | --- | --- |
| uwvm2 instruction | 5.100 / 5.273 / 5.300 (179) | 5.100 / 5.195 / 5.200 (180) |
| uwvm2 unwind | 5.000 / 5.200 / 5.300 (179) | 5.100 / 5.180 / 5.200 (181) |
| uwvm2-ros instruction | 5.000 / 5.200 / 5.200 (180) | 5.100 / 5.200 / 5.200 (181) |
| uwvm2-ros unwind | 4.965 / 5.100 / 5.200 (180) | 5.100 / 5.192 / 5.200 (181) |

The [EH raw controls](../../build/wasm3-evidence/performance-20261002-r9-small/r9-eh-development-diagnostic-20261002-r1/raw.jsonl)
use 200M normal steps and a separate 8M-step/500K-catch throw fixture.
The following times are internal guest seconds for the complete fixture,
not isolated throw latency. Normal and throw rows have different iteration
counts; subtracting the normal time from throw time is invalid.

| Product / trace | Plain normal, auto | EH normal, auto | Throws, auto | Throws, explicit native unwind |
| --- | ---: | ---: | ---: | ---: |
| uwvm2 instruction | 6.818274 | 6.882177 | 0.292983 | 2.703751 |
| uwvm2 unwind | 0.143815 | 0.143841 | 0.027191 | 3.385423 |
| uwvm2-ros instruction | 6.902518 | 6.841140 | 0.298914 | 2.717492 |
| uwvm2-ros unwind | 0.144440 | 0.148010 | 0.028493 | 3.233282 |

Both products' auto throws prove `pending-plan=r2-phase`; explicit native
throws prove `pending-plan=native`. All rows had zero managed GC collections.
The two auto-unwind throw controls and Wasmtime's 0.093196s process control
were below the 100ms quality floor. The unwind normal controls had only about
six log-envelope frequency points, and auto-unwind throws had one each.
Their semantic success and observed duration remain recorded, but they do
not establish frequency-qualified cross-engine performance. The instruction
normal controls' large cost is a concrete frame-recording investigation
input, not a universal speed ratio.

The [memory64 raw controls](../../build/wasm3-evidence/performance-20261002-r9-small/r9-memory-development-diagnostic-20261002-r1/raw.jsonl)
completed with matching self-checks. At 50M steps internal guest seconds
were 0.116217, 0.116795, 0.116832 and 0.112366 for ordinary instruction,
ordinary unwind, ROS instruction and ROS unwind respectively. Wasmtime
process time was 0.158437s. All five 5M process controls were below 100ms
and had no product execution-envelope frequency points. The 50M product
controls each had five points. This small round records correctness and
observed memory-access cost; it cannot establish a highest-performance claim.

The host observer was authenticated against each admitted guest's birth,
affinity `[0]` and actual cgroup; only that exact cgroup was excluded. Across
417 GC observer frames, one foreign active record last ran on CPU0; across
480 EH frames, two did. No explicit CPU0/1 foreign record appeared in the
six memory frames. Eligible broad-affinity activity remained ambiguous
(458, 518 and 19 records respectively). Actual observer interval medians
were approximately 147.350ms, 145.628ms and 172.808ms. These observations
do not prove host quiet or full SMT residency, and no foreign process was
signaled. Temperature remains observation only.
