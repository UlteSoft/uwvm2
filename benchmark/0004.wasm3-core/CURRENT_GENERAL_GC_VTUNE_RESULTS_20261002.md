# Current S6e general GC: actual hardware VTune results

This is the actual-result companion to the preserved [pre-launch recipe v2](CURRENT_GENERAL_GC_VTUNE_20261002.md). The old recipe's planned cpu-mask/filter templates and statement of no current result remain historical text; they are not retroactively rewritten as successful commands.

Primary recoverable evidence in both repositories is [actual-small-packet.tar.gz](../../build/wasm3-evidence/current-general-gc-vtune-complete-20261002-r1/actual-small-packet.tar.gz), 2,339,803 bytes, SHA44b539bfdf05f1a31d6d364e3286cb749d60e7157914c9302c6822e0fa8625c6, 187 members. Parent review SHA2795f03e72b1f52470cf01c717a925ee8ec30bf2216469bbe4e1d013e730607e separately closed actual guest P0/UID/birth, owned retirement and before/after byte-equivalent sources. The origin is /tmp/uwvm-r10-evidence-20261002/current-general-gc-vtune-complete-packet-20261002-r1/. The small packet preserves logs/receipts/reports/raw config; it does not replace the complete remote VTune result databases.

Four actual hardware collections completed under Intel VTune2026.4 build632893: reference-array-allocate2M and mutable-struct-mutate2M, each hardware Hotspots and uarch-exploration summary. Their PIDs/TIDs are respectively147969/147969,148332/148332,148619/148619,150115/150115. All real collection/report failures from earlier attempts remain failures in the packet. The completed report group is r6, not the failed filter attempts.

**-cpu-mask=0 was reported unsupported for this target type.** The target guest was independently pinned to P0; correct final report filtering uses the actual per-result PID, TID and **cpuid=cpu_0**, Core Type P-Core. Numeric cpuid=0 did not select the intended data. Unfiltered data contain other profiler/E-core observations and cannot be presented as pure P0 guest metrics.

The product is the ordinary S6e compatibility-map profile: SIDsha256:6e05059af0b6c2a46ed4476d82920d91636dcda66dc6e59360e0560a4fe05131, ELFb5f39dfe70e08a308a0cc415dd3e904386dd069d24a0cf330c75f860c0026830. Actual RT/main/host argv omit UWVM_USE_THREAD_LOCAL. In src/uwvm2/runtime/lib/uwvm_runtime.default.cpp, get_thread_state exists only in the !UWVM_USE_THREAD_LOCAL map branch. Its measured cost is **not evidence of the native-TLS default's cost**. New aligned native-TLS builds are independently qualified profiles; a before/after claim must hold TLS and every other profile value equal.

The four filtered CSV SHA values are:

| Report | Actual PID/TID | SHA256 |
| --- | --- | --- |
| reference-array allocate Hotspots | 147969 | 6a1d92ea71d72bfa390498d64a19c6e205e977bc83dcf17babd125b21ee5775a |
| reference-array allocate uarch | 148332 | f645f8b524b814135c61494aa742551224072551ad7843785cd202b1c2cf0f10 |
| mutable-struct mutate Hotspots | 148619 | f34b89f83f5e538e8d8fb71f57b095933c1902c470cee5a0a9c42642e0d53767 |
| mutable-struct mutate uarch | 150115 | f2b48ba66a990ed71d1d1df1da716676753cf193bcf914dd663a3f419cd87d89 |

Each address-bucket Hotspots row is aggregated by actual module path, full function, start address and range. These are sampled **self** buckets, not inclusive call-stack costs. Collector lambda numbers identify only this exact ELF's reported function; they cannot identify a future build. Source review of this actual collector maps lambda2 to locate. Sample-estimated instructions are not the independent perf-stat exact counters.

Reference-array allocation has3577 filtered PC rows, total sampled self CPU2.000932s:

| Actual named function in this ELF | Sampled self s | Share |
| --- | ---: | ---: |
| collector locate, start0x17a6a90/size0x235 | .315475 | 15.77% |
| map-fallback get_thread_state | .216265 | 10.81% |
| checked_aggregate_object | .119048 | 5.95% |
| array_get | .104163 | 5.21% |
| scalar struct_get32 bridge | .099203 | 4.96% |
| visit/preflight lambda, start0x17a62b0 | .082338 | 4.11% |
| boost rw_spinlock::lock_shared | .081347 | 4.07% |
| collector main | .076388 | 3.82% |
| publish | .063492 | 3.17% |
| retain_embedded_reference | .025792 | 1.29% |

Anonymous [ld.so.cache] contributes.091264s (4.56%) and remains unknown. No contemporaneous /proc/guest/maps was captured; module-path evidence is VTune's report, not an independent mapping closure. Neither those samples nor [Outside any known module] may be relabelled JIT functions. A finalized object alone cannot connect unknown sampled PCs to an actual loaded generation/range.

Mutable-struct mutation has586 filtered PC rows, total sampled self CPU.127968s: table_get .020832s, struct_set .012896s, scalar getter bridge .012896s, checked_aggregate .011904s, anonymous cache .011904s. This short series has coarse sample buckets; no tight relative ranking follows. It has only initialization allocations, zero collection attempts/reclamation and reason2phase_pending, so it qualifies field/lookup attribution only.

Both uarch summary function reports contain a single **Unknown** function row. P0 estimated counters are11,624,660,531 cycles /36,614,594,054 instructions for reference-array allocation, and566,138,634 /2,168,526,976 for mutation. Actual MUX is **.574/.584**; estimated/uarch values have that coverage limitation even though summary was already selected. Changing the label to summary does not remove multiplexing or produce named function uarch attribution. Unfiltered aggregate numbers can include other cores and are not substituted here.

The internal WASM execution timer excludes startup/JIT but includes guest setup/readback/checksum and all GC operations. VTune profiles a different execution; its CPU/self estimates and collector elapsed time do not replace the unprofiled wait4 measurements or exact100%-running whole-guest grouped hardware counters in [the independent results](CURRENT_GENERAL_GC_RESULTS_20261002.md). Temperature remains raw observation only; frequency/PMU coverage/identity/resource/noise limits remain explicit. No industry ranking or pure collector latency is claimed.

The first narrow production candidate is stopped-cohort collection-local token membership, based on the actual named locate hotspot and existing acquire-published local buckets. It retains original full graph preflight, synchronization, canonical pins, foreign/exn checks and error-zero-reclamation. Native-TLS investigation is separate. Neither this attribution nor papers demonstrate a candidate speedup; the defaultoff candidate requires real current cold and matched-profile A/B results.
