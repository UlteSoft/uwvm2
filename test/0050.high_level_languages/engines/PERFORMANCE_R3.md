The prepared cohort is on SSH Linux at
`/tmp/high-level-20261017/final/harness-python-native-budget-r3`. Preparation
started no guest or collector. Formal execution still requires the coordinator's
exclusive performance window and the Python 200k duration calibration.

The freeze SHA-256 is
`dc5dfa783492b05d269770d3261a3e78147aa9f5912a704dac21cc4893d2445f`.
Its 86 file hashes include the helpers, copied sampling runtime, manifests,
source/oracle/build receipts, environment and native ownership proof. The
record also checks 119 external runtime file identities. The existing readable
SDK/public aliases are reused through
`/tmp/high-level-20261017/profile-libraries-python-r1`; their receipt SHA-256 is
`842ca1b830aedb07695c2e39dab3f3c50d6451ba2391492bfcb4daa716b856e3`.
These aliases bind Wasm bytes, so the unchanged Python module is valid with the
new explicit 200k argument. The new manifest supplies its own checksum/oracle.

| Manifest | Composition | SHA-256 |
| --- | --- | --- |
| manifest-timed.json | Original 13 unchanged + Python 200k; 7 ordinary-long routes | 5b51fd8329705c2dd2b4a166d60ca5b775bf9f76940517da77ef14df89c2ca63 |
| manifest-gated.json | Original 6 unchanged + Python 200k; 7 routes | 28326c57a17d54be4834cdf642a8b38f8c613735ec65f88366916db5962a0d1e |

Only Python adds `--wasip1-noinherit-system-environment`. Its original pinned
O2 artifacts remain byte-identical: timed `c0376d1694688de46d6c8c34917c9915186e81b9fa7ef0d7c3622c980a3a267d`,
profile `0d0c5ce5bf911c4e8e44b760322e29d6e976e44c1a0d9b45c6a7d76ff7ad7e1d`.
Its native/Wasmtime reference receipt is
`b98d32f862faf3c946a334c6c227f6284d5df26943904e8ece7cb57073b81205`.
The expected checksum for 200k is 673761. The other six recipes, modules and
checks were not rewritten.

Hotspots uses event-config sa=20,000,000 and the unchanged requested 10ms
interval. Old real runsa options establish that this CLI uses an actual
sample-after multiplier of 10; the new runner checks the actual event bases
and multiplier after collection before accepting an effective 200,000,000
event period. This is an event count, not a measured 10ms temporal period.
SamplingSession's 10ms requirement, journal watch, CPU21/companion9, no stacks,
target capability checks, minimum 10 real retired/clock samples and gated
kernel/host limits are unchanged. Uarch uses detailed sampling, not summary
counting. No new sampling result is claimed by this preparation.

The owner fix is separately proven with real tiny native children, not Wasm.
It permits an initial unreaped direct leader with an ended pidfd only after
rechecking PID/start, Z state, ppid=self and pgrp=session=PID. That same
unreaped identity can discover still-live members of its original session/group.
Popen poll/reap immediately revokes this historical anchor; zombies remain
excluded from live ancestry and signal targets. The final proof SHA-256 is
`b4b4aa3b454de50d5647f453b0b647cb407f70e8f0713b4d38cb316c85996010`.
The tested helper SHA-256 is
`37d9d95d078bfbeb8fbca80dc060e2274d1b569589770ed06895825a3a36d9c9`.
Five proof groups cover direct Z/false and wrong start, real R-to-Z registration,
poll revocation, already-reaped refusal, same-group helper discovery, foreign
exclusion and invoke retaining N/A with successful cleanup. Processes which
fully detach and lose every observable ancestor/group anchor before discovery
or between scans remain outside this sampled ownership guarantee.

All preparation/probes ran on boot
`33464c92-7dae-4410-b9f7-92e3edf249b9`, in the original 64GiB/swap0 container.
Fixture processes retired and cgroup memory events stayed zero. The local
copies `harness-python-native-budget-r2/` and `harness-python-native-budget-r3/`
retain both freezes. R2's SHA remains
`0c6d59ceae136d50879b1706b9127370a2d4a20b818de663329151782072c5fe`.
The old-boot seven passed hardware receipts remain a separate incomplete
cohort and must never be combined with the planned 42 new profiles.

After release, run only the frozen scripts. The hardware matrix requires
initial driver affinity permitting both CPUs 9 and 21, and
`--ordinary-gated --capture-jit-metadata --profile-libraries /tmp/high-level-20261017/profile-libraries-python-r1`.
Request every one of the seven names in manifest-gated.json once; the defaults
select three repositories and Hotspots/uarch-detailed, producing 42 attempts.
The benchmark uses manifest-timed.json, recipes.json, CPU21 and the default
seven engines, producing 49 pairs (failed/unsupported pairs stay N/A). Both
scripts share `/tmp/uwvm-hll-benchmark.lock` and must run serially. Keep all
attempts immutable; a changed helper requires a new freeze and output cohort.

The separate static WAT diagnostic is in `python-control-layout-static-r1/`.
It streamed official wasm-tools output without storing full WAT. All 8841
defined bodies matched binary offsets and balanced block/loop/if/end controls;
42 imports are additional. It counted 1,610,742 lexical opcode sites and
sum(pre-control-depth)=15,459,258, maximum 295. An illustrative 104-byte object
factor gives 1,607,762,832 bytes, not actual allocator usage or peak RSS.
Unreachable sites, quotas, tuple/vector capacities, operand/local slots and
multiple stages prevent treating this as a measured checkpoint allocation.
The largest lexical function is index3098 `_PyEval_EvalFrameDefault`, with
33 declared locals, 30,822 sites and sum-depth5,043,530. This object estimate
alone does not establish the cause of a 52GiB debugger process.
