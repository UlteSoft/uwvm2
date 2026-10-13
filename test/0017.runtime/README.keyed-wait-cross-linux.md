# Cross-Linux wait, pause and close regression

`run_keyed_wait_cross_linux.py` compiles the real production pause domain and
keyed wait registry in both repositories. It executes actual native/QEMU
workers; this utility regression does not qualify a complete target VM,
LLVM JIT, Wasm data-stack preview or ASM debugger.

Each architecture runs four cases in each repository:

- Durable close: 40 cells, ordinary and prepared queues, two workers, two
  genuine pause/resume episodes, then close after resume. Passing requires
  physical worker joins, cancellation cleanup, drain, no remaining queue node
  and no repeated comparison.
- Old predicate: compile the same fixture with
  `UWVM2TEST_OLD_PAUSE_WAKE_PREDICATE`. It must return **1** with the explicit
  failure diagnostic after its notification rescue and physical joins. A
  timeout, assertion trap, signal or forced kill cannot pass this negative case.
- Suspension: actual queue order, pause observations, notification,
  cancellation and original finite-deadline arbitration.
- Prepared queues: saved queue order despite reversed native arrival,
  one-shot ready results, aborted startup cleanup, malformed-input refusal and
  deadlines starting only at the explicit batch open.

The runner verifies ELF class, endian, machine and the actual GNU_STACK flags.
Seven profiles require a non-executable stack. The unmodified Debian MIPS64
CRT and libc request an executable stack; this profile explicitly links with
that original contract and records flags 7. It does not receive a non-executable
stack qualification. No provider metadata is rewritten for passing execution.
It records actual phase arguments, return codes, logs, source identities,
compiler dependencies and provider hashes. Every selected source dependency
must match the frozen paired source manifest. Verified private provider files
are checked before and after execution. C++ fixture output uses FastIO.

Run only through the established SSH Linux owned-process resource supervisor
in the verified 64 GiB, swap-disabled test cgroup. The runner checks its actual
cgroup admission; it is not a replacement for the resource supervisor.
The supervisor must admit the exact compiler, linker, emulator and native
fixture paths while retaining ownership, memory, disk and timeout checks.

The provider root uses extracted cross packages at `usr/TRIPLE` and
`usr/lib/gcc-cross/TRIPLE/15`. PPC64 and SPARC64 use their real GNU cross
linkers at `usr/bin/TRIPLE-ld`; the other profiles use the supplied LLD.
MIPS64 needs both its cross Linux-header package and the matching common
Linux-header dependency. No packages need installation on the host.

Inside an admitted guarded command, an AArch64 invocation is:

```sh
python3 run_keyed_wait_cross_linux.py \
  --root /work/frozen-paired-source \
  --fixtures /work/frozen-paired-fixtures \
  --source-manifest /work/source-manifest.json \
  --providers /work/providers.json --deps /work/deps \
  --qemu /work/qemu/usr/bin --clang /usr/lib/llvm-22/bin/clang++ \
  --lld /work/toolchain/bin/ld.lld --profile aarch64 \
  --out /work/results/aarch64
```

The source manifest contains paired `identities` and relative `files` hashes;
the provider manifest contains `passed: true` and absolute `files` hashes.
Inputs and output directories must be immutable or exclusive to this run.
Other accepted profiles are x86_64, i686, riscv64, ppc64, mips64, sparc64 and
loongarch64. An accepted profile name alone is not an execution PASS.

The 2026-10-07 qualification completed all eight Linux profiles in both
repositories: 64 fresh compiles and 64 actual executions, comprising 48
successful cases and 16 expected old-predicate failures with genuine cleanup.
The close cases covered 640 two-worker cells, 1280 physical worker lifecycles
and 2560 worker pause observations. All commands ran under the original
64 GiB, swap-disabled SSH Linux cgroup. Source212 provides the frozen utility
headers; runner test260 qualifies seven profiles and test277 qualifies the
explicit original MIPS stack contract. No compiler/VM code was changed to
obtain a passing non-executable MIPS stack result. Full target VM/JIT/ASM
experience, whole-instance restore and performance remain separate obligations.

See [the exact qualification summary](../../documents/runtime/keyed-wait-cross-qualification-20261007.json)
for source identities, profile results, limits and the authenticated evidence
hash. Preparation failures, missing LLD runtime-library attempts and the
rejected MIPS stack-metadata experiment remain in the evidence history.
