# Current S6e general GC: actual development results

The recoverable primary evidence is [the repository packet](../../build/wasm3-evidence/current-general-gc-measurement-complete-20261002-r1/actual-small-packet.tar.gz), 1,506,191 bytes, SHA `67b03e8dc1c03d5bc24c461bd24f0006563f108ff0b134655419a3482e5eaecc`, retained in both repositories. `/tmp/uwvm-r10-evidence-20261002/current-general-gc-measurement-complete-packet-20261002-r1/` is its original download location. The parent independently reviewed original counts, identities and closures; its review SHA is `cc4cab1df20f8116493859084bc28647e0cdbb715c212f32ef6290b351c8f1da`. Launcher receipts describe resource caps; they do not contain a final initializer-only roster, which is not inferred here. This analysis independently reparses all 40 guest logs, all 8 original perf stat/debug logs, grouped event math/FD attributes/ACK/owned completion, and both canonical before/after source fingerprints. Review JSON is mirrored under `build/wasm3-evidence/current-general-gc-independent-review-20261002-r1/independent-review.json`.

Both series completed and their source/product/dependency closures matched before and after: unprofiled 32/32 semantic samples, pure hardware 8/8 semantic and exact-count samples. This is the ordinary S6e six-experiment-plus-SET32 r5 baseline only, external SID `sha256:6e05059af0b6c2a46ed4476d82920d91636dcda66dc6e59360e0560a4fe05131`, product SHA `b5f39dfe70e08a308a0cc415dd3e904386dd069d24a0cf330c75f860c0026830`. Default-off, ROS and the newer shared status-dispatch candidate are not measured here.

Actual S6e runtime/main/host argv omit UWVM_USE_THREAD_LOCAL: get_thread_state belongs to the compatibility map fallback, not the native-TLS default. The new all-TU native-TLS build is a separate profile and is not a patch-only A/B against S6e. Four actual hardware VTune samples and their corrected cpu_0 filtering, named sampled-self attribution and Unknown/MUX limits are recorded separately in [actual VTune results](CURRENT_GENERAL_GC_VTUNE_RESULTS_20261002.md).

The internal WASM timer excludes startup and JIT compilation, but includes setup, field accesses, checksums and all collections. Parent wait4 wall/user/sys/RSS cover the complete child. Hardware counters cover the whole single guest TID including startup/JIT, user plus kernel; their ROI differs from the internal timer. None is a pure collector latency. VTune is a third separate family and is not merged into these counts.

One observed sample per cell is retained without a ranking. Temperature is observation only; external SMT activity is unknown. Controller-window frequency P05/median/P95 is in the JSON; those wall-clock telemetry points have no same-point monotonic anchor for an exact WASM ROI. Short reference rows keep their missing identity/frequency evidence and explicit quality failures. No skipped identities are fabricated.

| 2M outer iterations | Unprofiled internal WASM ms | Parent wall ms | Parent user/sys ms | Parent peak RSS KiB | HW whole-TID instructions | HW whole-TID cycles | Actual collections/reclaimed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| mutable-struct / allocate | 368.027 | 391.494 | 395.813 / 4.997 | 51220 | 6153618718 | 2163635793 | 488 / 1997823 |
| mutable-struct / mutate | 78.611 | 104.207 | 107.844 / 5.991 | 50052 | 2156259079 | 551464254 | 0 / 0 |
| reference-cycle / allocate | 1002.547 | 1043.188 | 1044.562 / 7.996 | 52696 | 18958464510 | 5722725959 | 976 / 3995646 |
| reference-cycle / mutate | 267.457 | 304.432 | 307.436 / 5.989 | 51480 | 7065359491 | 1684016056 | 0 / 0 |
| numeric-array / allocate | 548.147 | 586.054 | 590.036 / 4.991 | 52820 | 11667377359 | 3196599732 | 488 / 1997823 |
| numeric-array / mutate | 121.865 | 153.674 | 157.096 / 5.965 | 51452 | 3582266204 | 837182962 | 0 / 0 |
| reference-array / allocate | 2040.226 | 2100.883 | 2098.692 / 10.998 | 55140 | 36607570124 | 11559956880 | 1464 / 5993469 |
| reference-array / mutate | 306.336 | 352.810 | 358.359 / 3.992 | 53080 | 8092453371 | 1906352207 | 0 / 0 |

Hardware is an independently executed sample, not counts from the unprofiled timer above. Every hardware row independently has PMU type4/config 0x3c and 0xc0, successful leader FD5 and member FD7 with group_fd5, exact enabled==running (100%), no scaling/inheritance/sampling, enabled ACK before guest GO, and original owned PIDFD SIGINT after real guest retirement. Exact per-row enabled/running and CPI remain in the JSON.

Allocation has exact 2M/4M/2M/6M allocations, enabled collection, reason0 and positive real reclamation. Mutation has only 1024/2048/1024/3072 setup allocations, zero attempts/collections/reclamation and reason2 (`phase_pending`); its qualification is field/lookup only and `collector_qualified=false`. `roots_requested=1` is the roots-enablement flag, not a measurement of one root or the table-slot count.

The same binary Wasmtime49 copying controls all completed their scalar selfchecks. Its observed parent wall times below are diagnostic data; most are below 100ms and cannot establish an industry ranking or trustworthy matched frequency distribution.

| Same-byte 2M case | Wasmtime parent wall ms | Quality failures |
| --- | ---: | --- |
| mutable-struct / allocate | 25.049 | Sub100ms or incomplete whole guest |
| mutable-struct / mutate | 5.591 | Sub100ms or incomplete whole guest; Missing in-window frequency; Unqualified short guest: original loaded executable/argv was not observed; identity remains unknown |
| reference-cycle / allocate | 48.597 | Sub100ms or incomplete whole guest |
| reference-cycle / mutate | 6.823 | Sub100ms or incomplete whole guest; Missing in-window frequency; Unqualified short guest: original loaded executable/argv was not observed; identity remains unknown |
| numeric-array / allocate | 37.340 | Sub100ms or incomplete whole guest |
| numeric-array / mutate | 11.387 | Sub100ms or incomplete whole guest; Missing in-window frequency; Unqualified short guest: original loaded executable/argv was not observed; identity remains unknown |
| reference-array / allocate | 133.738 | No recorded sample-quality failure; SMT unknown, one sample |
| reference-array / mutate | 14.467 | Sub100ms or incomplete whole guest; Missing in-window frequency; Unqualified short guest: original loaded executable/argv was not observed; identity remains unknown |

Three actual bounded-terminal receipts closed on original PIDFD readiness with actual Z/absent observations. They are source-checked in the independent review; the earlier failed attempts remain failed and are not rewritten. These safeguards affect observation and cleanup, not GC semantics or guest code generation.

Next attribution work separates named allocation/reservation/publication/root-tracing helpers from anonymous JIT PCs. Anonymous `[ld.so.cache]` samples stay unknown until the exact captured object, loaded section/symbol range, native generation and sample interval match. Allocation-versus-mutation differences above do not isolate allocator or collection time because their operation/readback counts differ. The subsequent actual named VTune report makes stopped-cohort collector membership the first separate GC candidate; success-first status lowering remains its own fresh aligned-codegen candidate. Same-operation aggregate-source authentication reuse is deferred because its measured retain_embedded helper is much smaller than collector locate. No lock, foreign lease, token lookup or whole-graph preflight is removed on the basis of this table.
