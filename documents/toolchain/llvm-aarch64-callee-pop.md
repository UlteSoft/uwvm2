# AArch64 callee-pop caller frames

A normal call to a guaranteed-tail function may return with SP advanced by the
callee's stack-argument size. With a reserved caller frame, LLVM must undo that
advance before it reloads a caller spill slot. AArch64 previously restored SP
at `ADJCALLSTACKUP`; register allocation can place a frame-index reload between
the call and that pseudo instruction. The reload consequently read a different
slot, although the final frame size and the callee's result were correct.

The repair moves the existing SP adjustment immediately after the call, matching
the established X86 policy. It does not change argument layout, the pop size,
ordinary call-frame accounting, memory checks or the runtime's unwind policy.
The retained MIR checks direct and indirect callee-pop calls and an ordinary
call with no callee-pop adjustment. Both little-endian and big-endian machine
verification are covered; the executable probe is little-endian AArch64 Linux.

## Provider and cache contract

The bundled provider advertises `LLVM_UWVM_AARCH64_CALLEE_POP_SP_FIXED=1` only
with the repaired source inventory. Its package revision is
`23.1.1-uwvm-ros.15`. ROS requires that exact provider and capability. Ordinary
UWVM can use an independently qualified external backport with
`UWVM_LLVM_AARCH64_CALLEE_POP_SP_FIXED=1`; the definition does not install a
compiler repair. Rebuild the actual LLVM CodeGen archives and qualify them
before enabling it. A version string or a patched header is insufficient.

Unqualified ordinary providers retain their established raw/C calling
convention; existing mandatory-tail checks still reject transfers that cannot
be materialized. ROS continues to require checked native unwind and never
selects the public instruction-stack policy as a substitute.

Every AArch64 object and checkpoint key records the callee-pop admission
revision, the effective fixed/unfixed capability, and explicit backport opt-in.
Other architectures do not gain these AArch64-only fields. Executable SHA-256,
LLVM/source/archive checks and authenticated cache envelopes remain required.
A provider or ABI change must miss the old cache; unchanged compatible code
may reuse its authenticated object. Live ROS/WASI permissions are still checked
in the new process and are never restored from cached code.

## Run the maintained regression

Build `llc` and `FileCheck` from the exact repaired LLVM source. The driver is
`test/0014.llvm_jit/run_llvm23_aarch64_callee_pop.py`; its adjacent fixtures are
part of the test input. Run it on Linux inside the existing serialized test
lease and an actual 64 GiB, zero-swap cgroup:

```sh
python3 test/0014.llvm_jit/run_llvm23_aarch64_callee_pop.py \
  --llc /opt/qualified-llvm/bin/llc \
  --filecheck /opt/qualified-llvm/bin/FileCheck \
  --clang /opt/bootstrap/bin/clang \
  --tool-runner tool-runner.json \
  --target-runner target-runner.json \
  --compiler-flags compiler-flags.json \
  --out /work/aarch64-callee-pop-results
```

The optional JSON files contain explicit argument arrays, never shell commands.
`tool-runner.json` prefixes `llc` and `FileCheck` when those are target binaries.
`target-runner.json` prefixes the AArch64 executable, for example a qualified
QEMU user-mode invocation with its target loader/sysroot. Empty arrays select
native execution. Compiler flags supply the target sysroot and link inputs.
All tools, fixtures, commands, raw streams and final probe hashes are recorded.
The output directory must be new.

An unchanged old provider must fail both MIR expectations and execute with a
wrong direct or indirect spill value while the ordinary control remains 44.
`--expect-broken` records that negative control. The repaired executable must
print exactly `direct=42 indirect=43 ordinary=44`. LLVM 23.0 used the PEI registry
name `prologepilog`; select it explicitly with `--pei-pass prologepilog` when
qualifying that exact external provider. The bundled 23.1 provider uses
`prolog-epilog`.

The portable LLVM patch is
[llvm-aarch64-callee-pop.patch](patches/llvm-aarch64-callee-pop.patch). Apply it
at the LLVM project root and rebuild; the runtime capability remains a separate
admission decision. The ROS and master-stable bundled trees contain the repair;
the ordinary master branch uses an external LLVM provider.

## Retained evidence and scope

The original QuickJS-backed KUKA memory64 guest has SHA-256
`bdc93a619bf17729f9f6a72c2c4975e0b1acd503bc5f248b357a92e221cf3bc3`.
It reached a false memory-bounds trap even though the reconstructed access
`0x7ffa68 + 8` was inside the `0xc40000`-byte memory. The earlier caller spill
reload used the displaced SP; the cold diagnostic block subsequently reloaded
the correct value after SP restoration. No memory-bound check was relaxed.

The unchanged diagnostic provider reproduced the failure. Its rebuilt backend
then passed the machine/executable controls and the same original guest under
`pb-o1`, `legacy-light` and `pb-o3`. The raw receipt archive is SHA-256
`eb30add4b5ed55576008ae3db7e1c19ede81670201a026761baa8f438e0ca49a`.
That incremental diagnostic executable is not a release build. A fresh paired
provider and full-system VM qualification is recorded separately in the ROS
runtime report. These correctness runs do not measure native performance,
qualify a big-endian ROS deployment or certify every external LLVM release.
