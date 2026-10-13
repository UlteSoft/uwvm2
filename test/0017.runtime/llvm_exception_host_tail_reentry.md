# Host-tail adapter exception trace regression

This fixture executes the complete LLVM JIT runtime. Its host import uses the
public raw host API to reenter the same prepared module. It does not inject a
native exception, substitute an LLVM IR probe, or use debugger APIs.

The generated Wasm has these function indices:

| Index | Function | Role |
| --- | --- | --- |
| 0 | `reenter` | Actual registered host import |
| 1 | `leaf` | Throws tag payload `i32 37` when requested |
| 2 | `retired` | Tail-calls import 0 inside a `try_table catch_all` |
| 3 | `entry` | Calls `retired` normally and has a side effect after return |
| 4 | `normal_marker` | Reads that side effect for the nonthrowing control |

Both direct and indirect tail transfers are checked. On normal return, exactly
one host callback must return and the surviving entry's side effect must occur.
On the throwing path, the nested public host boundary must report the original
Wasm stack `[1, 3]` for direct transfer and `[1, 0, 3]` for indirect transfer
under both `instruction` and `unwind`. The indirect bridge retains the active
host import identity 0. Function 2 has retired;
its catch must not run, and its host adapter is not another Wasm activation.
The payload, actual callback marker and nested entry identity are also checked,
so an unrelated crash does not count as a successful death test.

Compile `llvm_exception_host_tail_reentry.cc` on SSH Linux against the exact
frozen runtime object and LLVM libraries used by the corresponding fresh CLI.
Use the successful runtime test compile/link flags for that product; preserve
its C++ exception ABI, library provider and build macros. The test source already
selects the strict LLVM runner and supports ordinary and ROS full builds.
Do not compile or execute this test locally.

After the frozen r241 inputs and execution window are supplied, run inside the
configured 64 GiB cgroup, once per product:

```sh
python3 test/0017.runtime/run_llvm_exception_host_tail_reentry.py \
  --probe /dev/shm/qualified-product/host-tail-reentry \
  --runtime-object /dev/shm/qualified-product/runtime.o \
  --wasm-tools /dev/shm/uwvm-tools/wasm-tools-1.259.0 \
  --product uwvm2 \
  --out /dev/shm/host-tail-reentry-ordinary
```

For ROS select `--product uwvm2-ros` and its separately linked probe/runtime.
The runner validates the new syntax with `wasm-tools`, then performs eight
process-isolated VM executions. It records both policies even when one fails,
retains bounded logs and exact frame sequences, disables core dumps, and hashes
the probe, runtime object, fixture generator and Wasm tool before and after.
An attribution defect produces a failing summary and exit status 1; it is never
relabeled as a passing expected failure. Link commands and frozen source identity
must accompany the runner output in the surrounding qualification evidence.

The r242 baseline reused the r241 O3 runtime objects after both products' complete
production source fingerprints matched. It reproduced the retired adapter frame:

| Product | Transfer | Instruction frames | Unwind frames before repair |
| --- | --- | --- | --- |
| ordinary | direct | `[1, 3]` | `[1, 2, 3]` |
| ordinary | indirect | `[1, 0, 3]` | `[0, 1, 2, 3]` |
| ROS | direct | `[1, 3]` | `[1, 2, 3]` |
| ROS | indirect | `[1, 0, 3]` | `[1, 2, 3]` |

The original runner also rejected the normal `instruction/source locations
unavailable` diagnostic phrase and omitted the active indirect host import from
its expected frames. Its raw logs and original failing summaries remain archived;
the corrected runner requires the precise frame sequences above and rejects only
a missing backtrace warning.

The selective r243 production repair was rebuilt at O3 on x86_64 Linux. Both
products passed all eight host-reentry executions. Direct stacks are now `[1, 3]`
and indirect stacks `[1, 0, 3]` under both policies. The existing 31 new-syntax
numeric EH cases additionally passed ordinary full/lazy and ROS full, each under
instruction/unwind: 186 VM executions plus 128 wasm-tools commands. Including the
new host-reentry fixture, this qualification contains 202 successful VM executions.
All compilation/execution used the 64 GiB cgroup, zero swap and CPUs
`0,2,4,6,16-31`; production source fingerprints matched before and after execution.
This is native Linux semantic qualification, not a QEMU or performance result.

Complete evidence is in `build/wasm3-evidence` in the ordinary repository:

- `host-tail-reentry-r242-baseline.tar.xz`, SHA-256
  `6aad5bc1997545944e6590427a0109a8b052a9ddc143e3492637e8040d6265aa`.
- `host-tail-reentry-r243-qualified.tar.xz`, SHA-256
  `3669e5e3e092abc5092de1de513909e897f9313361a41d7fedbb655f19715ee3`.
  All 552 recorded files were individually verified against `SHA256SUMS.json`.
  This includes the exact O3 commands, compiler/library provenance, source
  overlays, source fingerprints, binaries/runtime objects, fixtures and logs.

The frozen r243 source IDs are
`975399904f7e29c97208e0d38c2f4515ce3350bd66101aca77ee49c6038cd5a2`
for ordinary and
`7f44d9d0812f9febb636f29a35920dd7139cfa94736a1236ac42630e51743f8d`
for ROS. `r243-build-host-tail.py` in the evidence directory derives the complete
compile/link commands from r241 provenance and records every changed source ID.
