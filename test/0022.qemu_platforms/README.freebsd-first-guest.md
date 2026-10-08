# FreeBSD 15.1 cold SDK and serial guest queue

The official `base.txz` provider is pinned to 164,624,792 bytes and SHA-256
`3768988b151c20f965679062b065c63a977d6bbb9f47fd83695ec2c40790c18f`.
The actual R2 download passed that checksum and then failed because a selected
SDK link resolves outside the selected provider subtree. Keep its original
source, log, exit status and task retirement. The approved read-only link census
reads that same archive in the shared cgroup and records the exact refused
links; it neither extracts files nor grants permission to omit links. The next
SDK closure must be chosen from that real inventory and must bind every actual
compiler dependency and CRT/C++/libc/loader provider. The actual one-stage
census completed successfully with 30,469 members and 281 selected links. It
identified one refusal: `usr/lib/libxo/encoder/test.enc` names
`/usr/tests/lib/libxo/libenc_test.so`. Its original PIDFD child was reaped, the
shared cgroup returned to init only, and the original ticket retired.
`freebsd_sdk_link_census_r1_actual_20261003.json` records that narrow result.
The reviewed exact-alias extraction then completed its one actual stage in
8.52 seconds with a 68,198,400-byte peak RSS. It copied 484,593,691 regular
bytes from the existing checksum-pinned archive, excluded exactly that one
test alias after matching its type and target, and retained every other strict
link check and byte limit. The original child and ticket retired with the
cgroup back to init only. `freebsd_sdk_exact_alias_r1_actual_20261003.json`
records the result. This qualifies the selected dataset extraction; target
compilation, linked provider closure and guest execution remain unqualified.

The first 27-command cold SDK oracle group stopped at input-before: the SDK
provider's `usr/lib/include` is a directory alias with a null file checksum,
and the test binder incorrectly treated it as a regular file. Only that one
Python stage executed; no compiler, ELF tool or guest ran. The original
PIDFD child and shared ticket retired, with the cgroup back to init only.
`freebsd_sdk_oracle_cold_r1_actual_failure_20261003.json` preserves the result.
A separate read-only metadata audit covers all 280 retained links and confirms
exactly one typed directory alias. The independent R2 proposal binds its actual
link text, private canonical directory and link/target identities before and
after, while retaining regular-file hashes and all original budgets. The reviewed independent R2-planfix group then completed all 27 actual stages
in 40.64 seconds with a 375,701,504-byte peak RSS. Four fresh x64 FreeBSD
OSABI9 ELFs were compiled and linked against actual MD dependencies, CRT,
SDK C++/libc/loader and recursive DT_NEEDED bytes. The actual compiler reports
23.0.0git, not 23.1.1. All original root/known PIDFDs retired and the ticket
returned with init-only/64GiB/swap0/OOM0.
`freebsd_sdk_oracle_cold_r2_planfix_actual_20261003.json` binds 65 preserved
original raw/object/artifact files. This qualifies only those standalone SDK
header components and their recorded target runtime closure. Guest/kernel,
named modules, symbol-version binding, ROS bundled LLVM products, native
debugger and whole VM restore remain unqualified.

The clean guest comes from the existing official BASIC-CLOUDINIT UFS compressed
image, not the historical customized 7 GB disk. Its compressed payload is
664,729,340 bytes and must match the official checksum before and after a
bounded materialization. The clean QCOW2 is exactly 2,671,443,968 bytes. The
reviewed one-command image stage permits a 3 GiB output file, preserves the
original 64 GiB/swap0 cgroup and PIDFD retirement checks, and requires an
8 GiB free-space reserve plus all SDK/overlay/ISO/evidence budgets. The signed
checksum text is retained; this queue has not cryptographically verified its
PGP signature. No compiler, qemu-img, mount or guest runs in this stage.

`freebsd151_sdk_kernel_oracle.cc` uses the real FreeBSD target headers. It
records the SDK stat layout and kernel identity, independently calls the SDK's
`__xuname(SYS_NMLN, record)` and `fstat` through noexcept ELF assembler symbols,
and checks fast_io-owned regular files against that native result. It seeks
to 8 GiB while keeping the file size zero, writes canonical little-endian and
LEB bytes, checked-closes the writer and reads with `native_file_loader`.
The scalar fsync result is only a real kernel API observation; it does not
prove filesystem power-loss durability or qualify a production BSD checkpoint
backend. The probe must execute as the isolated non-root guest user.

The official FreeBSD cloud image uses **nuageinit**. The source preparation in
`freebsd151_serial_seed.py` produces only a fixed CIDATA directory from four
actual, independently bound x64 ELF artifacts. It emits small plain-text
cloud-config and places each ELF in the ISO as a separate file. Its later
fixed guest script mounts that ISO read-only, verifies the actual ELF hashes,
copies them into a private guest directory and invokes all four profiles as
an unprivileged user. This avoids feeding megabyte executables through the
nuageinit base64 bit-string decoder. It suppresses first-boot package upgrades
and sshd, requires no SSH password/key and carries no guest network interface.
The explicit root provisioning script is private to this disposable test
image; it is not a product debugger or runtime permission interface.

The proposed first VM has 4 GiB guest RAM, four E-core vCPUs, a private QCOW2
overlay capped at 512 MiB, a CIDATA ISO capped at 16 MiB, no NIC, no display and
a private QMP socket. Serial output is an owned regular file capped at 32 MiB.
`freebsd151_serial_controller.py` supplies the finite inner controller source.
It opens the original QEMU child PIDFD before any fallible provenance read,
checks QMP `query-kvm` and the actual four vCPU thread identities, and accepts
only ordered fresh-nonce serial results from all four actual bound ELFs. It
checks the public QCOW2 backing-name fields against the one read-only clean
base, refuses extra/deleted mapped providers, and reaps its original child.
The existing outer PIDFD guardian and actual init-only lane retirement remain
additional requirements; the inner receipt does not publish platform acceptance. Both the clean image and firmware/host ELF providers must remain
bound to their actual hashes. Existing Windows/BSD VM controllers carry old
boot/process identities and cannot be reused as current authority.

Those VM/controller/ISO stages remain unexecuted source preparations. A clean
image, prepared seed, cross-built ELF or boot alone is not a PASS. Only actual
probe output with its exact four artifact/provider bindings can qualify this
cold SDK component. Full ROS LLVM23 products, native debugger stepping,
Wasm Core 3 new syntax, live instance checkpoints and other architectures
require their separate current-source runs.

Primary references: [FreeBSD 15.1 release](https://www.freebsd.org/releases/15.1R/),
[official basic cloud image configuration](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/15.1/release/tools/basic-cloudinit.conf),
[nuageinit implementation](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/15.1/libexec/nuageinit/nuageinit),
[nuage user/file implementation](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/15.1/libexec/nuageinit/nuage.lua),
[FreeBSD stat SDK](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/15.1/sys/sys/stat.h),
[FreeBSD uname SDK](https://raw.githubusercontent.com/freebsd/freebsd-src/releng/15.1/sys/sys/utsname.h).

The controller has only been parsed as source. It has not launched QEMU. A
complete fixed provider/ISO/overlay/guardian manifest must be independently
reviewed and bound before its first owned guest run. QMP and disk parsing use
[QEMU QMP protocol](https://www.qemu.org/docs/master/interop/qmp-spec.html),
[CPU QAPI](https://raw.githubusercontent.com/qemu/qemu/v10.2.1/qapi/machine.json),
and [the official QCOW2 header format](https://raw.githubusercontent.com/qemu/qemu/v10.2.1/docs/interop/qcow2.rst).

The independent R2 metadata finalizer subsequently rechecked all 27 original
successful receipts, four actual ELF objects/binaries and seven exact target
runtime providers. Its fresh 9,935-byte output has SHA-256
`e6a0f31823819ca265eab8b206e7778fae77627198ac93d1146e6ef145f92d97`.
The R1 metadata rejection remains preserved: the original successful guardian
omits `cancelled`, and R2 checks that exact successful schema. No compiler,
loader, guest or new native tool ran in this metadata step.
`freebsd_finalizer_r2_metadata_actual_20261003.json` binds this limited result.
The pending runtime-verifying seed will check actual guest library bytes before
its fresh serial nonce; the VM qualification remains false.

The first reviewed clean-image ONE stopped before archive hashing or any XZ
materialization. Its fixed free-space check required 13,120,502,168 bytes,
while the actual post-failure filesystem snapshot had 12,384,907,264 bytes.
The original 10,063,872-byte peak-RSS child exited 1 and its PIDFD was ready;
all tracked children were reaped, the 64 GiB/swap0 cgroup returned to init only
with no OOM event, and the shared ticket retired. No new QCOW2 or guest was
created. `freebsd_clean_image_r1_actual_failure_20261003.json` binds all eight
preserved source/log/receipt/ticket files. The reserve remains 8 GiB; a later
run must first resolve the real disk budget without hiding this failure.
