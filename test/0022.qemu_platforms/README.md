# Current-source QEMU correctness queue

This directory and `tools/qemu-platform-tests/` belong to the QEMU platform
test agent. They do not replace the existing Linux keeper's native, P-core,
hardware-counter, or VTune qualification. The shared execution ticket and
the existing original PIDFD supervisor must both admit every build and guest
in the same 64 GiB, swap-free Docker cgroup. No compiler or guest runs on the
Mac mini as part of this queue.

Checkpoint format 3, owner source R4, passed the first actual big-endian cold
suite: ROS and ordinary uwvm2, each with exceptions enabled and disabled.
All 30 guarded stages exited zero. Each new ELF64 MSB EM_S390 executable ran
under qemu-s390x with `--require-big`; all four outputs matched the independent
Python model (4,347 bytes, SHA-256
`731ddd5ee0febc08055ac4f19faf34fd63af7f0ea9690b658622c5dabee9ff01`).
The new suite pins distribution Clang22, its 18 recursive ELF host providers
and resource headers, the GCC15 target sysroot, actual compile dependencies
and link-map providers. It keeps the complete immutable fast_io header closure
and four explicitly approved scalar SHA header repairs. The earlier R3 source
compile failure and R4 installed-Clang23 SystemZ-backend failure remain in
separate records. The full result is in
`checkpoint_s390x_clang22_r1_actual_20261003.json`; compilation alone or a
missing compiler backend never establishes a target execution PASS.

The standalone checkpoint test checks its canonical wire data, corruption and
truncation rejection, reference identity/cycles, 64-bit memory/table offsets,
exceptions, thread/wait state and replay records. It is **not** a live VM
continuation restore, reverse execution or deterministic replay qualification.
Its target C++ provider is the existing GCC15 libstdc++, so it is **not** a
qualification of the new paired libc++/libc++abi/libunwind ABI 1 product build.
Fresh runtime/main/host translation units and a target-specific LLVM/runtime
closure remain necessary for a product result.

Next source queues are new Core 3 validator/int join cases, GC inline metadata
and actual object/root-store cases, fast_io API/module consumers, LLVM loaded
provenance and native exception/thread/debugger cold cases. An existing
AArch64/RISC-V64/i386 result is historical evidence for its exact source and
binary. It cannot qualify newly added syntax, ABI, source bindings or state.

Windows uses the present guest image after fresh asset hashing and private
overlay preparation. FreeBSD uses the official compressed clean image after
its full official SHA verification and bounded private materialization. The Windows disk retains its
original virtio-scsi controller and read-only ROM/template; a new private
variable store, overlay and input ISO are required. Windows loader errors,
guest boot success, PE compilation and actual new-syntax execution are distinct
results. FreeBSD filesystem ABI requires its actual SDK and independent
oracle; the Linux/Darwin-only public POSIX extension does not establish BSD
stat compatibility. Unsupported checkpoint file synchronization remains
unavailable until its provider is implemented and tested.

The Windows RAII component packet freezes both identical owner fixtures,
backend/register/Win32-ABI leaves and its complete immutable fast_io headers.
The R3 cold run produced one fresh ROS EH AMD64 PE, bound its actual compiler
dependencies/linker Reading inputs and collected import/unwind/bridge data.
Its noEH compilation exposed unguarded fast_io shared-memory try/throw cleanup.
All raw failures remain preserved; the four approved RAII header repairs are
rebased onto a separate R4 source packet. The original R3 PE imports GNU
libgcc_s_seh-1.dll and is not a self-contained LLVM runtime qualification.
A new independent SDK-static LLVM four-cell recipe is prepared with
compiler-rt/libunwind; it still requires actual fresh outputs and real guest
VEH/GPR/cancellation/death probes. A separate fresh
runtime/main/host build is required to qualify the products' current debugger
ABI. The native filesystem/provider inventory in
`non_native_provider_inventory_20261003.json` identifies 15 Linux target
loader/libc/emulator candidates; it records no platform execution acceptance.

FreeBSD SDK R2 actually downloaded the official 164,624,792-byte base.txz
and passed its SHA check, then rejected a link outside the selected SDK subset.
The original owned process was reaped, its PIDFD became readable, the cgroup
returned to init-only and the shared ticket retired without OOM. The result in
`freebsd_sdk_r2_actual_failure_20261003.json` remains a failure. A separate
metadata-only census of that original SHA-pinned archive locates the exact
links before any SDK closure adjustment. A bounded SDK/kernel probe and
CIDATA serial transport are source preparations; neither a prepared sysroot,
a cross-built ELF nor a guest boot is a BSD execution acceptance.

The final requested matrix includes operating system, architecture, byte order,
libc and product/mode combinations. A missing target LLVM, OpenSSL, C++ ABI,
startup object or SDK closure remains pending and is not a PASS. This queue
records real command arguments, failures, exit codes, source/tool/provider and
object hashes, raw logs, original task retirement and actual resource limits.

References: [Clang cross compilation](https://clang.llvm.org/docs/CrossCompilation.html),
[QEMU user-mode execution](https://www.qemu.org/docs/master/user/main.html),
[FreeBSD 15.1 release information](https://www.freebsd.org/releases/15.1R/).
