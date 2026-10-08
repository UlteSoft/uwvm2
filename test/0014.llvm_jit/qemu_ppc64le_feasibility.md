# PPC64LE ROS9 QEMU preflight (2026-09-24)

This is a read-only feasibility audit, not a PPC64LE qualification result.
Inside the 64 GiB, swap-disabled Linux test cgroup, the dependency tree has
`/work/deps/usr/bin/qemu-ppc64le` (QEMU 11.1.1), the
`powerpc64le-linux-gnu` GCC 15 C++ headers, startup objects, `libatomic.a`,
dynamic loader `ld64.so.2`, and a target sysroot. The native ros.9 LLVM
configuration includes `PowerPC`; its PowerPC CodeGen, AsmParser,
Disassembler, Desc, and Info archives exist under
`/dev/shm/uwvm-llvm-ros9/build/lib`.

There is no ros.9 **PPC64LE target-architecture** LLVM build or qualified
`consumer-link.rsp` under `/dev/shm`. The native archives cannot be linked
into a PPC64LE VM. The sysroot also lacks PPC64LE `libssl.so.3` and
`libcrypto.so.3`, which the current full-JIT VM link requires. Thus the
existing AArch64/RV64 cross-build script cannot yet build PPC64LE.

The smallest qualification path is to build PPC64LE ros.9 LLVM archives from
the pinned source manifest, verify every archive member and the complete
consumer link response, add the PPC64LE OpenSSL runtime closure, then extend
`build_ros9_cross_vm_reuse.py` for `powerpc64le-linux-gnu`/`EM_PPC64` and
the PPC loader. The QEMU EH/threads runner must recognize the PPC target,
execute instruction and unwind policies, capture actual MCJIT ELF objects,
and derive PPC atomic/call assembly assertions from those objects. It must
also check whether process-local addresses make signed object replay unsafe
before enabling any persistent cache assertion. Estimated from the existing
AArch64 (548 MiB) and RV64 (896 MiB) target LLVM builds: about 0.5–0.9 GiB
for another target archive tree and roughly 16–26 GiB peak single-build
memory, subject to measurement. At audit time `/dev/shm` had 2.3 GiB free
and the host work filesystem had 1.4 GiB free. No PPC build or QEMU test was
started.
