# Linux cross-provider recovery and linker regression — 2026-10-06

Both repositories now accept independent `--deps-root` and `--qemu-root`
provider roots in `run_debug_linux_qemu_components.py`. Omitting
`--qemu-root` preserves the rule that QEMU is found below `--deps-root`.
The default dependency root remains
`/home/macromodel/Documents/uwvm3-implementation/deps`.

A private extracted ppc64 GNU linker failed before producing an ELF because
its native `libbfd-2.46-ppc64.so` was outside the inherited library search path.
For a linker below the dependency root, the recipe now prepends the private
`usr/lib/x86_64-linux-gnu` directory to a copied environment for the
compile/link subprocess. It hashes those native library providers before
and after the run. The QEMU guest environment is still set explicitly to
the target sysroot/GCC directories. The native linker library path is never
added to the ppc64 guest path. The x86_64 LLD path keeps the inherited
compile environment.

The executable recipe is identical in both repositories:
`c75dd784667a9ad4ee48b4369e183684638d0186439c3e0c7dfe892054859351`.
This increment changes the test recipe and documents its evidence; it does
not modify C++ runtime code, IO implementation or ROS execution modes.

## Actual guarded results

| Recipe / target | uwvm2 | uwvm2-ros |
| --- | --- | --- |
| Private SDK + independent QEMU, ppc64 / big endian / ELF64 | 3 / 3 | 3 / 3 |
| Private SDK + independent QEMU, x86_64 / little endian / ELF64 | 3 / 3 | 3 / 3 |
| Omitted QEMU-root option, x86_64 DAP component | 1 / 1 | 1 / 1 |

All 14 components passed. The three custom-root components are
`debug_source_boolean_cast`, `debug_source_zig_category` and
`debug_source_dap_expression`. Each actual QEMU output equals the pinned
x86_64 native component reference. x86_64 target ELFs additionally run
directly and compare their native and QEMU output bytes.

Totals are 5,181,268 Boolean cast checks, 988,636 category checks and
1,212 positive DAP corpus result rows. The six guard receipts passed,
12,724 input-hash verifications completed, and no OOM events increased.
Peak aggregate owned RSS was 763,211,776 bytes; maximum observed owned
output was 8,442,072 bytes. Six guarded jobs took 204.007 seconds in total.
These timings are observations of a shared test environment.

Concurrent source updates were frozen separately. Successful production
cuts were:

- uwvm2: `sha256:b5d602209e5f91d8344ad582ab4198d8c1bc573e96a6afea246d08f9a6a9333a`
- uwvm2-ros: `sha256:cbf51f1ce95fd01fde24005340f29c5ea68f489199c166664704609aa303a9ec`

A further control uses the actual pre-loader-fix recipe
`3a4b27d62d438092277e6254fcc2e5f1420eb63d13a14e3f81a5e9d9b5a37347`
on the exact successful uwvm2 source cut and the same private SDK. Its
Boolean fixture compiles but the link fails with missing native libbfd;
no QEMU execution occurs. All 822 compiler dependencies match the
positive run. The expected-failure controller and its guard passed.
This isolates the loader repair from concurrent production edits.

The first actual three-row linker failure is preserved. An underscore
in an x86_64 task label was rejected before admission; corrected labels
were resumed separately. The first negative-control checker incorrectly
requested a post-link result field after the expected failed link.
Its failed guard and traceback are preserved; the corrected checker
validates the real dependency file and passed in a fresh output directory.

## Environment and provider ownership

Every compilation and executable test ran through the birth/PIDFD-bound
owned-process supervisor in shared container
`uwvm-debug-tests64g-20261006-r2`, with memory.max 64 GiB, swap.max zero,
and worker CPUs 16–31. DATA jobs retain 6 GiB shared headroom, a 1 GiB
aggregate owned RSS budget, 64 MiB output limit, 8 MiB file limit,
1 MiB log limit, 1 GiB actual filesystem reserve and 4096 free inodes.
The 25 GiB host disk reserve and existing 9 GiB full-product admission
gates remain in place.

Persistent output is on the restored 16 GiB bounded ext4 arena.
Its sparse image allocated 609,497,088 bytes at evidence verification,
including earlier retained work; it had 16,258,138,112 free filesystem
bytes. Other agents' processes, SDKs and artifacts were not cleaned up.

The ppc64 SDK was extracted into the private first-attempt directory
from nine pinned official Ubuntu packages; no system package installation
or maintainer scripts ran. Downloads total 12,422,580 bytes, provider
regular files total 80,168,579 bytes, and all 2777 provider files and all
nine package archive hashes were checked again. The SDK extraction
guard passed with peak RSS 44,097,536 bytes.

QEMU is reused read-only from
`/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/qemu`.
Package version is `1:10.2.1+ds-1ubuntu3.2`; its manifest and the actual
ppc64 and x86_64 binaries are independently pinned. The omitted-option
test uses a private dependency view containing a symlink to that same
pinned x86_64 QEMU binary.

When constructing an owned supervised component job, the new recipe
arguments are, for example:

```text
--root <frozen-paired-source-cut>
--out <new-private-output-directory>
--profiles ppc64 --repositories uwvm2
--deps-root <private-sdk-root>
--qemu-root <independently-pinned-qemu-root>
--cases debug_source_boolean_cast,debug_source_zig_category,debug_source_dap_expression
```

The recipe's cgroup check supplements the supervisor. Builds and target
execution must use the owned supervisor and its resource admission.

## Retained evidence and scope

Main Linux archive:
`/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/cross-provider-recovery-a2/cross-provider-recovery-evidence-a1.tar.xz`

Local verified copy:
`/Users/liyinan/.codex/artifacts/uwvm2-cross-provider-recovery-evidence-20261006-a1/cross-provider-recovery-evidence-a1.tar.xz`

It is 64,405,376 bytes with 15,467 regular members. SHA-256:
`fabce1d3219f8a80a6723a7f9738ed53dfb0315a695892a51eef134f417c460f`.
Every member hash was verified independently on Linux and locally.
It retains both source cuts, private SDK packages/files, actual recipe
wrappers, target ELFs, all main logs/receipts, provider hashes, pinned
native reference logs and copied ppc64/x86_64 QEMU binaries. Final
sidecars retain the same-cut negative controls, their failed checker,
final paired files, review and copy proofs.

[Machine-readable qualification](debug_cross_provider_recovery_20261006.json)
records the exact cuts, guards and external archive proofs.
This round qualifies these frozen scalar/type/DAP DATA components on
ppc64 and x86_64. Other target SDKs have not been restored by this work.
It does not establish all 16 architecture parity, full uwvm CLI behavior,
live producer/native language debugger parity, hot replacement, or ASM
guest-context execution. The older pre-reboot ppc64 failure had no
retained raw logs; it is not assigned the cause of this new observed
loader failure.
