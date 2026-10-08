# Native numeric exception performance observations

These results measure the frozen ordinary r212 and ROS r213 numeric native EH
implementations on SSH Linux. They are not measurements of later tiered
publication or helper-attribute changes, complete Core 3 acceptance, or a claim
that one stack policy is universally faster.

Both runtime/CLI builds use Clang O3 and genuine LLVM 23.1.1-uwvm-ros.8; generated
full-JIT code explicitly uses `pb-o3`. All sampling ran sequentially in the
64 GiB/no-swap cgroup on allowed CPU 0, with other agents' remote compilation and
tests suspended. The cgroup CPU allowance remains `0,2,4,6,16-31`. Process/cgroup
snapshots, complete command lines, build fingerprints, executable hashes and raw
samples are retained in `native-exception-performance-r215c`.

## Normal execution and actual propagation

Each policy has three comparisons with nine alternating AB/BA pairs. Samples
last at least 100 ms and 20 times measured empty process startup. Startup and
compilation remain included. Each guest checks its exact integer endpoint; the
same workload generator is separately checked by Wasmtime with at most 200,000
iterations. Reference counts are recorded and are not claimed to match larger
UWVM calibration counts.

Median paired ratios B/A for the exact same ordinary-call program with exceptions
enabled versus disabled, and for protected normal versus ordinary calls, are:

| Product / trace policy | EH enabled / disabled | Protected / ordinary |
| --- | ---: | ---: |
| Ordinary instruction | 1.0101 | 1.0063 |
| Ordinary unwind | 1.0006 | 1.0115 |
| Ordinary none | 1.0146 | 0.9954 |
| ROS instruction | 0.9851 | 1.0282 |
| ROS unwind | 0.9952 | 1.0063 |
| ROS none | 1.0010 | 0.9999 |

The medians are close in these workloads; individual samples vary. Values below
one are not evidence of a general optimization benefit. Ordinary-call medians
in separately calibrated policy runs were about 44 ns per iteration with
instruction frames and 0.9–1.0 ns with unwind/none. They include the arithmetic
loop and amortized process work, and are not an instruction-versus-unwind AB/BA
comparison. The generated-code check separately proves that unwind omits logical
push/pop and leaves the mmap memory-loop bytes and relocations unchanged.

## Matched diagnostic policies during throwing

A separate nine-pair experiment runs instruction and unwind against identical
Wasm bytes and the same 50,000-iteration count. These values include immutable
exception value construction, allocations, enabled throw-site diagnostic
capture, tag matching, native propagation and cleanup. They do not isolate the
native unwinder.

| Product / throwing depth | Instruction, us/throw | Unwind, us/throw | Paired unwind / instruction |
| --- | ---: | ---: | ---: |
| Ordinary, one callee | 8.91 | 14.93 | 1.6716 |
| Ordinary, eight callees | 17.39 | 16.65 | 0.9576 |
| ROS, one callee | 6.11 | 9.57 | 1.5620 |
| ROS, eight callees | 17.65 | 15.91 | 0.9221 |

Unwind reduces work on normal calls by avoiding logical frame maintenance. It
still has to walk native CFI when the exception is thrown; in these shallow
throwing cases that costs more than copying the already-maintained instruction
trace. It is therefore incorrect to call every throw faster in unwind mode.
Ordinary/ROS absolute times and earlier separately calibrated runs vary, so the
matched ratios are the appropriate limited comparison within each experiment.

The `none` policy still performs real native guest exception propagation. Its
separate depth-one/depth-eight medians are approximately 5.12/9.00 us ordinary
and 5.05/8.96 us ROS; it does not capture a diagnostic stack and is not substituted
for either measured diagnostic policy.

## Retained failed calibration and limits

The first attempt, `native-exception-performance-r215b`, completed the ordinary
instruction normal-path pairs but over-scaled the local static throw fixture to
1,417,276,430 iterations. UWVM lowers that throw directly to a branch, while the
reference still executes exceptions. The excessive Wasmtime reference process
was stopped; that iteration is retained with its nonzero exit and is not marked
passed. The final runner bounds reference work independently and selects the
three comparisons reported above. Local static lowering has an exact complete
executable-section/relocation comparison instead; no omitted timing is counted
as a pass.

The earlier `r215` orchestration attempt stopped before measurements because its
compiler-process guard counted zombie compiler processes. The corrected guard
still rejects active compiler processes and records zombies in the raw snapshot.

Actual VM thread creation/entry/return is measured by the separate thread
lifecycle harness. These exception measurements do not qualify thread creation,
all platforms, all modes, reference exceptions, or the debugger.

The verified archive is `build/wasm3-evidence/core3-native-exception-performance-r215.tar.gz`
(1,097 files), SHA-256
`23ce197c4fe15fcfac1868c9aa75fa14efe4bff98d1a320aa99b4a6143c23621`.
It includes successful measurements, both stopped orchestration/calibration
iterations, input Wasm/WAT, raw samples and the exact runners.

## Later controlled r231 O3 measurements

The r231 ordinary and ROS source snapshots were rebuilt with host `-O3` and
generated `pb-o3`; ROS used genuine bundled LLVM `23.1.1-uwvm-ros.9`. Both
actual CLIs passed three fresh Core 3 exception fixtures and eight native
exception/mmap machine-code checks over ten cached objects. Nine AB/BA pairs
per comparison showed no large normal-path penalty from merely enabling the
exception feature in these workloads: median enabled/disabled ratios ranged
from 0.9835 to 1.0183. Protected/ordinary normal-call ratios ranged from
1.0018 to 1.0262. Identical-Wasm paired throw tests gave unwind/instruction
ratios of 1.7209 (one callee) and 0.9755 (eight callees) ordinary, and 1.6116
and 0.8552 ROS. This confirms that native unwind saves normal-path logical
frame maintenance without making every actual throw faster. Each sample
amortized process startup by at least 20 times and lasted at least 100 ms;
diagnostic capture, propagation, startup and JIT compilation remained included.

Full commands, source fingerprints, raw samples and verified file hashes are
in `build/wasm3-evidence/ordinary-o3-r231.tar.xz` (SHA-256
`804cc77100ca244f80643483bd402aa26bed86d2534c286f0c6bca17032bc288`)
and `build/wasm3-evidence/ros-o3-r231.tar.xz` (SHA-256
`93f1147fb5eddcf8f01d25ccbf8b92a0890f4390b7c2d51fffdecaab320a94ba`).
These are focused controlled-r231 observations; newer local source and the
remaining language/platform matrix need separate qualification.
